/**
 * Class for input, output and processing of Page XML files and referenced image.
 *
 * @version $Version: 2018.06.22$
 * @copyright Copyright (c) 2016-present, Mauricio Villegas <mauricio_ville@yahoo.com>
 * @license MIT License
 */

#include "PageXML.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdexcept>
#include <regex>
#include <iomanip>
#include <unordered_set>
#include <cassert>

#include <opencv2/opencv.hpp>
#include <libxml/xpathInternals.h>
//#include <libxslt/xslt.h>
#include <libxslt/xsltconfig.h>

using namespace std;

const char* PageXML::settingNames[] = {
  "indent",
  "pagens",
  "grayimg"
};

char default_pagens[] = "http://schema.primaresearch.org/PAGE/gts/pagecontent/2013-07-15";

#ifdef __PAGEXML_MAGICK__
Magick::Color transparent("rgba(0,0,0,0)");
Magick::Color opaque("rgba(0,0,0,100%)");
#endif
regex reXheight(".*x-height: *([0-9.]+) *px;.*");
regex reRotation(".*readingOrientation: *([0-9.]+) *;.*");
regex reDirection(".*readingDirection: *([lrt]t[rlb]) *;.*");
regex reFileExt("\\.[^.]+$");
regex reInvalidBaseChars(" ");
regex reImagePageNum("(^.*)\\[([0-9]+)]$");
regex reIsPdf(".*\\.pdf(\\[[0-9]+])*$",std::regex::icase);


/////////////////////
/// Class version ///
/////////////////////

static char class_version[] = "Version: 2018.06.22";

/**
 * Returns the class version.
 */
char* PageXML::version() {
  return class_version+9;
}

void PageXML::printVersions( FILE* file ) {
  fprintf( file, "compiled against PageXML %s\n", class_version+9 );
  fprintf( file, "compiled against libxml2 %s, linked with %s\n", LIBXML_DOTTED_VERSION, xmlParserVersion );
  fprintf( file, "compiled against libxslt %s, linked with %s\n", LIBXSLT_DOTTED_VERSION, xsltEngineVersion );
  fprintf( file, "compiled against opencv %s\n", CV_VERSION );
}


/////////////////////////
/// Resources release ///
/////////////////////////

/**
 * Releases all reserved resources of PageXML instance.
 */
void PageXML::release() {
  if( xml == NULL )
    return;

  if( xml != NULL )
    xmlFreeDoc(xml);
  xml = NULL;
  if( context != NULL )
    xmlXPathFreeContext(context);
  context = NULL;
  if( sortattr != NULL )
    xsltFreeStylesheet(sortattr);
  sortattr = NULL;
  xmlDir = string("");
#if defined (__PAGEXML_LEPT__)
  for( int n=0; n<(int)pagesImage.size(); n++ )
    pixDestroy(&(pagesImage[n]));
#endif
  pagesImage = std::vector<PageImage>();
  pagesImageFilename = std::vector<std::string>();
  pagesImageBase = std::vector<std::string>();
  process_running = NULL;
}

/**
 * PageXML object destructor.
 */
PageXML::~PageXML() {
  release();
}

////////////////////
/// Constructors ///
////////////////////

PageXML::PageXML() {
  if( pagens == NULL )
    pagens = default_pagens;
}

#if defined (__PAGEXML_LIBCONFIG__)

/**
 * PageXML constructor that receives a libconfig Config object.
 *
 * @param config  A libconfig Config object.
 */
PageXML::PageXML( const libconfig::Config& config ) {
  loadConf(config);
  if( pagens == NULL )
    pagens = default_pagens;
}

/**
 * PageXML constructor that receives a configuration file name.
 *
 * @param cfgfile  Configuration file to use.
 */
PageXML::PageXML( const char* cfgfile ) {
  if( cfgfile != NULL ) {
    libconfig::Config config;
    config.readFile(cfgfile);
    loadConf(config);
  }
  if( pagens == NULL )
    pagens = default_pagens;
}

#else

/**
 * PageXML constructor that receives a file name to load.
 *
 * @param fname  File name of the XML file to read.
 */
PageXML::PageXML( const char* fname ) {
  if( pagens == NULL )
    pagens = default_pagens;
  loadXml( fname );
}

#endif


//////////////
/// Output ///
//////////////

/**
 * Writes the current state of the XML to a file using utf-8 encoding.
 *
 * @param fname  File name of where the XML file will be written.
 * @return       Number of bytes written.
 */
int PageXML::write( const char* fname ) {
  if ( process_running )
    processEnd();
  xmlDocPtr sortedXml = xsltApplyStylesheet( sortattr, xml, NULL );
  int bytes = xmlSaveFormatFileEnc( fname, sortedXml, "utf-8", indent );
  xmlFreeDoc(sortedXml);
  return bytes;
  //return xmlSaveFormatFileEnc( fname, xml, "utf-8", indent );
}

/**
 * Creates a string representation of the Page XML.
 */
string PageXML::toString() {
  string sxml;
  xmlChar *cxml;
  int size;
  xmlDocDumpMemory(xml, &cxml, &size);
  if ( cxml == NULL ) {
    throw_runtime_error( "PageXML.toString: problem dumping to memory" );
    return sxml;
  }
  sxml = string((char*)cxml);
  xmlFree(cxml);
  return sxml;
}


/////////////////////
/// Configuration ///
/////////////////////

#if defined (__PAGEXML_LIBCONFIG__)

/**
 * Gets the enum value for a configuration setting name, or -1 if unknown.
 *
 * @param format  String containing setting name.
 * @return        Enum format value.
 */
inline static int parsePageSetting( const char* setting ) {
  int settings = sizeof(PageXML::settingNames) / sizeof(PageXML::settingNames[0]);
  for( int n=0; n<settings; n++ )
    if( ! strcmp(PageXML::settingNames[n],setting) )
      return n;
  return -1;
}

/**
 * Applies configuration options to the PageXML instance.
 *
 * @param config  A libconfig Config object.
 */
void PageXML::loadConf( const libconfig::Config& config ) {
  if( ! config.exists("PageXML") )
    return;

  const libconfig::Setting& pagecfg = config.getRoot()["PageXML"];

  int numsettings = pagecfg.getLength();
  for( int i = 0; i < numsettings; i++ ) {
    const libconfig::Setting& setting = pagecfg[i];
    //printf("PageXML: setting=%s enum=%d\n",setting.getName(),parsePageSetting(setting.getName()));
    switch( parsePageSetting(setting.getName()) ) {
      case PAGEXML_SETTING_INDENT:
        indent = (bool)setting;
        break;
      case PAGEXML_SETTING_PAGENS:
        if( pagens != NULL && pagens != default_pagens )
          free(pagens);
        pagens = strdup(setting.c_str());
        break;
      case PAGEXML_SETTING_GRAYIMG:
        grayimg = (bool)setting;
        break;
      default:
        throw invalid_argument( string("PageXML.loadConf: unexpected configuration property: ") + setting.getName() );
    }
  }
}

#endif

/**
 * Prints the current configuration.
 *
 * @param file  File to print to.
 */
void PageXML::printConf( FILE* file ) {
  fprintf( file, "PageXML: {\n" );
  fprintf( file, "  indent = %s;\n", indent ? "true" : "false" );
  fprintf( file, "  pagens = \"%s\";\n", pagens );
  fprintf( file, "  grayimg = %s;\n", grayimg ? "true" : "false" );
  fprintf( file, "}\n" );
}


///////////////
/// Loaders ///
///////////////

/**
 * Creates a new Page XML.
 *
 * @param creator  Info about tool creating the XML.
 * @param image    Path to the image file.
 * @param imgW     Width of image.
 * @param imgH     Height of image.
 */
xmlNodePt PageXML::newXml( const char* creator, const char* image, const int imgW, const int imgH ) {
  release();

  time_t now;
  time(&now);
  char tstamp[sizeof "YYYY-MM-DDTHH:MM:SSZ"];
  strftime(tstamp, sizeof tstamp, "%FT%TZ", gmtime(&now));

  string str = string("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n")
      + "<PcGts xmlns=\"" + pagens + "\">\n"
      + "  <Metadata>\n"
      + "    <Creator>" + (creator == NULL ? "PageXML.cc" : creator) + "</Creator>\n"
      + "    <Created>" + tstamp + "</Created>\n"
      + "    <LastChange>" + tstamp + "</LastChange>\n"
      + "  </Metadata>\n"
      + "  <Page imageFilename=\"" + image +"\" imageHeight=\"" + to_string(imgH) + "\" imageWidth=\"" + to_string(imgW) + "\"/>\n"
      + "</PcGts>\n";

  xmlKeepBlanksDefault(0);
  xml = xmlParseDoc( (xmlChar*)str.c_str() );
  setupXml();

  if( imgW <= 0 || imgH <= 0 ) {
#if defined (__PAGEXML_LEPT__) || defined (__PAGEXML_MAGICK__) || defined (__PAGEXML_CVIMG__)
    loadImage( 0, NULL, false );
#if defined (__PAGEXML_LEPT__)
    int width = pixGetWidth(pagesImage[0]);
    int height = pixGetHeight(pagesImage[0]);
#elif defined (__PAGEXML_MAGICK__)
    int width = pagesImage[0].columns();
    int height = pagesImage[0].rows();
#elif defined (__PAGEXML_CVIMG__)
    int width = pagesImage[0].size().width;
    int height = pagesImage[0].size().height;
#endif
    setAttr( "//_:Page", "imageWidth", to_string(width).c_str() );
    setAttr( "//_:Page", "imageHeight", to_string(height).c_str() );
#else
    throw_runtime_error( "PageXML.newXml: invalid image size" );
    release();
    return NULL;
#endif
  }

  return selectNth( "//_:Page", 0 );
}

/**
 * Loads a Page XML from a file.
 *
 * @param fname  File name of the XML file to read.
 */
void PageXML::loadXml( const char* fname ) {
  release();

  if ( ! strcmp(fname,"-") ) {
    loadXml( STDIN_FILENO, false );
    return;
  }

  size_t slash_pos = string(fname).find_last_of("/");
  xmlDir = slash_pos == string::npos ?
    string(""):
    string(fname).substr(0,slash_pos);

  FILE *file;
  if ( (file=fopen(fname,"rb")) == NULL ) {
    throw_runtime_error( "PageXML.loadXml: unable to open file: %s", fname );
    return;
  }
  loadXml( fileno(file), false );
  fclose(file);
}

/**
 * Loads a Page XML from an input stream.
 *
 * @param fnum      File number from where to read the XML file.
 * @param prevfree  Whether to release resources before loading.
 */
void PageXML::loadXml( int fnum, bool prevfree ) {
  if ( prevfree )
    release();

  xmlKeepBlanksDefault(0);
  xml = xmlReadFd( fnum, NULL, NULL, XML_PARSE_NONET );
  if ( ! xml ) {
   throw_runtime_error( "PageXML.loadXml: problems reading file" );
   return;
  }
  setupXml();
}

/**
 * Loads a Page XML from a string.
 *
 * @param xml_string  The XML content.
 */
void PageXML::loadXmlString( const char* xml_string ) {
  release();

  xml = xmlParseDoc( (xmlChar*)xml_string );
  if ( ! xml ) {
   throw_runtime_error( "PageXML.loadXml: problems reading XML from string" );
   return;
  }
  setupXml();
}

/**
 * Populates the imageFilename and pagesImageBase arrays for the given page number.
 */
void PageXML::parsePageImage( int pagenum ) {
  xmlNodePt page = selectNth( "//_:Page", pagenum );
  string imageFilename = getAttr( page, "imageFilename" );
  if( imageFilename.empty() ) {
    throw_runtime_error( "PageXML.parsePageImage: problems retrieving image filename from xml" );
    return;
  }
  pagesImageFilename[pagenum] = imageFilename;

  string imageBase = regex_replace( imageFilename, reFileExt, "" );
  imageBase = regex_replace( imageBase, reInvalidBaseChars, "_" );
  pagesImageBase[pagenum] = imageBase;
}

/**
 * Setups internal variables related to the loaded Page XML.
 */
void PageXML::setupXml() {
  context = xmlXPathNewContext(xml);
  if( context == NULL ) {
    throw_runtime_error( "PageXML.setupXml: unable create xpath context" );
    return;
  }
  if( xmlXPathRegisterNs( context, (xmlChar*)"_", (xmlChar*)pagens ) != 0 ) {
    throw_runtime_error( "PageXML.setupXml: unable to register namespace" );
    return;
  }
  rootnode = context->node;
  rpagens = xmlSearchNsByHref(xml,xmlDocGetRootElement(xml),(xmlChar*)pagens);

  vector<xmlNodePt> elem_page = select( "//_:Page" );
  if( elem_page.size() == 0 ) {
    throw_runtime_error( "PageXML.setupXml: unable to find Page element(s)" );
    return;
  }

  pagesImage = std::vector<PageImage>(elem_page.size());
  pagesImageFilename = std::vector<std::string>(elem_page.size());
  pagesImageBase = std::vector<std::string>(elem_page.size());

  for( int n=0; n<(int)elem_page.size(); n++ )
    parsePageImage(n);

  if( xmlDir.empty() )
    xmlDir = string(".");

  if( sortattr == NULL )
    sortattr = xsltParseStylesheetDoc( xmlParseDoc( (xmlChar*)"<xsl:stylesheet xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" version=\"1.0\"><xsl:output method=\"xml\" indent=\"yes\" encoding=\"utf-8\" omit-xml-declaration=\"no\"/><xsl:template match=\"*\"><xsl:copy><xsl:apply-templates select=\"@*\"><xsl:sort select=\"name()\"/></xsl:apply-templates><xsl:apply-templates/></xsl:copy></xsl:template><xsl:template match=\"@*|comment()|processing-instruction()\"><xsl:copy/></xsl:template></xsl:stylesheet>" ) );
}

#if defined (__PAGEXML_LEPT__) || defined (__PAGEXML_MAGICK__) || defined (__PAGEXML_CVIMG__)

/**
 * Function that creates a temporal file using the mktemp command
 *
 * @param tempbase    The mktemp template to use, including at least 3 consecutive X.
 */
void mktemp( const char* tempbase, char *tempname ) {
  char cmd[FILENAME_MAX];
  sprintf( cmd, "mktemp %s", tempbase );
  FILE *p = popen( cmd, "r" );
  if( p != NULL ) {
    sprintf( cmd, "%%%ds\n", FILENAME_MAX-1 );
    if( fscanf( p, cmd, tempname ) != 1 )
      tempname[0] = '\0';
    pclose(p);
  }
}

/**
 * Loads an image for a Page in the XML.
 *
 * @param pagenum        The number of the page for which to load the image.
 * @param fname          File name of the image to read, overriding the one in the XML.
 * @param resize_coords  If image size differs, resize page XML coordinates.
 * @param density        Load the image at the given density, resizing the page coordinates if required.
 */
void PageXML::loadImage( int pagenum, const char* fname, const bool resize_coords, const int density ) {
  string aux;
  if( fname == NULL ) {
    aux = pagesImageFilename[pagenum].at(0) == '/' ? pagesImageFilename[pagenum] : (xmlDir+'/'+pagesImageFilename[pagenum]);
    fname = aux.c_str();
  }

#if defined (__PAGEXML_MAGICK__)
  cmatch base_match;
  if( std::regex_match(fname,base_match,reImagePageNum) ) {
    aux = base_match[1].str() + "[" + std::to_string(stoi(base_match[2].str())-1) + "]";
    fname = aux.c_str();
  }
#endif

#if defined (__PAGEXML_LEPT__)
#if defined (__PAGEXML_MAGICK__)
  if( std::regex_match(fname, reIsPdf) ) {
    int ldensity = density;
    if( ! density ) {
      if( resize_coords )
        throw_runtime_error( "PageXML.loadImage: density is required when reading pdf with resize_coords option" );
      Magick::Image ptmp;
      ptmp.ping(fname);
      double Dw = 72.0*getPageWidth(pagenum)/ptmp.columns();
      double Dh = 72.0*getPageHeight(pagenum)/ptmp.rows();
      ldensity = std::round(0.5*(Dw+Dh));
    }
    Magick::Image tmp;
    tmp.density(std::to_string(ldensity).c_str());
    tmp.read(fname);
    char tmpfname[FILENAME_MAX];
    std::string tmpbase = std::string("tmp_PageXML_pdf_")+std::to_string(pagenum)+"_XXXXXXXX.png";
    mktemp( tmpbase.c_str(), tmpfname );
    tmp.resolutionUnits(MagickCore::ResolutionType::PixelsPerInchResolution);
    tmp.write( (std::string("png24:")+tmpfname).c_str() );
    pagesImage[pagenum] = pixRead(tmpfname);
    unlink(tmpfname);
  }
  else
    pagesImage[pagenum] = pixRead(fname);
#else
  pagesImage[pagenum] = pixRead(fname);
#endif
  if( pagesImage[pagenum] == NULL ) {
    throw_runtime_error( "PageXML.loadImage: problems reading image: %s", fname );
    return;
  }
#elif defined (__PAGEXML_MAGICK__)
  try {
    if( density )
      pagesImage[pagenum].density(std::to_string(density).c_str());
    pagesImage[pagenum].read(fname);
  }
  catch( exception& e ) {
    throw_runtime_error( "PageXML.loadImage: problems reading image: %s", e.what() );
    return;
  }
#elif defined (__PAGEXML_CVIMG__)
  pagesImage[pagenum] = grayimg ? cv::imread(fname,CV_LOAD_IMAGE_GRAYSCALE) : cv::imread(fname);
  if ( ! pagesImage[pagenum].data ) {
    throw_runtime_error( "PageXML.loadImage: problems reading image: %s", fname );
    return;
  }
#endif

  if( grayimg ) {
#if defined (__PAGEXML_LEPT__)
    Pix *orig = pagesImage[pagenum];
    pagesImage[pagenum] = pixConvertRGBToGray(orig,0.0,0.0,0.0);
    pixDestroy(&orig);
#elif defined (__PAGEXML_MAGICK__)
    if( pagesImage[pagenum].matte() && pagesImage[pagenum].type() != Magick::GrayscaleMatteType )
      pagesImage[pagenum].type( Magick::GrayscaleMatteType );
    else if( ! pagesImage[pagenum].matte() && pagesImage[pagenum].type() != Magick::GrayscaleType )
      pagesImage[pagenum].type( Magick::GrayscaleType );
#endif
  }

#if defined (__PAGEXML_LEPT__)
  int imgwidth = pixGetWidth(pagesImage[pagenum]);
  int imgheight = pixGetHeight(pagesImage[pagenum]);
#elif defined (__PAGEXML_MAGICK__)
  int imgwidth = (int)pagesImage[pagenum].columns();
  int imgheight = (int)pagesImage[pagenum].rows();
#elif defined (__PAGEXML_CVIMG__)
  int imgwidth = pagesImage[pagenum].size().width;
  int imgheight = pagesImage[pagenum].size().height;
#endif
  int width = getPageWidth(pagenum);
  int height = getPageHeight(pagenum);

  /// Resize XML coords if required ///
  if( ( width != imgwidth || height != imgheight ) && resize_coords ) {
    xmlNodePt page = selectNth("//_:Page",pagenum);
    resize( cv::Size2i(imgwidth,imgheight), page, true );
    width = getPageWidth(page);
    height = getPageHeight(page);
  }

  /// Check that image size agrees with XML ///
  if( width != imgwidth || height != imgheight )
    throw_runtime_error( "PageXML.loadImage: discrepancy between image and xml page size (%dx%d vs. %dx%d): %s", imgwidth, imgheight, width, height, fname );

  /// Check image orientation and rotate accordingly ///
  int angle = getPageImageOrientation( pagenum );
  if ( angle ) {
#if defined (__PAGEXML_LEPT__)
    Pix *orig = pagesImage[pagenum];
    if ( angle == 90 )
      pagesImage[pagenum] = pixRotateOrth(orig,1);
    else if ( angle == 180 )
      pagesImage[pagenum] = pixRotateOrth(orig,2);
    else if ( angle == -90 )
      pagesImage[pagenum] = pixRotateOrth(orig,3);
    pixDestroy(&orig);
#elif defined (__PAGEXML_MAGICK__)
    int width_orig = pagesImage[pagenum].columns();
    int height_orig = pagesImage[pagenum].rows();
    int width_rot = angle == 180 ? width_orig : height_orig;
    int height_rot = angle == 180 ? height_orig : width_orig;
    Magick::Image rotated( Magick::Geometry(width_rot, height_rot), Magick::Color("black") );
    Magick::Pixels view_orig(pagesImage[pagenum]);
    Magick::Pixels view_rot(rotated);
    const Magick::PixelPacket *pixs_orig = view_orig.getConst( 0, 0, width_orig, height_orig );
    Magick::PixelPacket *pixs_rot = view_rot.get( 0, 0, width_rot, height_rot );
    if ( angle == 90 ) {
      for( int y=0, n=0; y<height_orig; y++ )
        for( int x=0; x<width_orig; x++, n++ )
          pixs_rot[x*width_rot+(width_rot-1-y)] = pixs_orig[n];
    }
    else if ( angle == 180 ) {
      for( int y=0, n=0; y<height_orig; y++ )
        for( int x=0; x<width_orig; x++, n++ )
          pixs_rot[(height_rot-1-y)*width_rot+(width_rot-1-x)] = pixs_orig[n];
    }
    else if ( angle == -90 ) {
      for( int y=0, n=0; y<height_orig; y++ )
        for( int x=0; x<width_orig; x++, n++ )
          pixs_rot[(height_rot-1-x)*width_rot+y] = pixs_orig[n];
    }
    view_rot.sync();
    pagesImage[pagenum] = rotated;
#elif defined (__PAGEXML_CVIMG__)
    PageImage rotated;
    if ( angle == 90 ) {
      cv::transpose(pagesImage[pagenum], rotated);
      cv::flip(rotated, rotated, 1); //transpose+flip(1)=CW
    }
    else if ( angle == 180 ) {
      cv::flip(pagesImage[pagenum], rotated, -1); //flip(-1)=180
    }
    else if ( angle == -90 ) {
      cv::transpose(pagesImage[pagenum], rotated);
      cv::flip(rotated, rotated, 0); //transpose+flip(0)=CCW
    }
    pagesImage[pagenum] = rotated;
#endif
  }
}

void PageXML::loadImage( xmlNodePt node, const char* fname, const bool resize_coords, const int density ) {
  int pagenum = getPageNumber(node);
  if( pagenum >= 0 )
    return loadImage( pagenum, fname, resize_coords, density );
  throw_runtime_error( "PageXML.loadImage: node must be a Page or descendant of a Page" );
}

void PageXML::loadImages( const bool resize_coords, const int density ) {
  int numpages = count("//_:Page");
  for( int n=0; n<numpages; n++ )
    loadImage( n, NULL, resize_coords, density );
}


#endif

///////////////////////
/// Page processing ///
///////////////////////

#ifdef __PAGEXML_MAGICK__

/**
 * Converts an cv vector of points to a Magick++ list of Coordinates.
 *
 * @param points   Vector of points.
 * @return         List of coordinates.
 */
list<Magick::Coordinate> cvToMagick( const vector<cv::Point2f>& points ) {
//list<Magick::Coordinate> PageXML::cvToMagick( const vector<cv::Point2f>& points ) {
  list<Magick::Coordinate> mpoints;
  for( int n=0; n<(int)points.size(); n++ ) {
    mpoints.push_back( Magick::Coordinate( points[n].x, points[n].y ) );
  }
  return mpoints;
}

#endif

/**
 * Parses a string of pairs of coordinates (x1,y1 [x2,y2 ...]) into an array.
 *
 * @param spoints  String containing coordinate pairs.
 * @return         Array of (x,y) coordinates.
 */
vector<cv::Point2f> PageXML::stringToPoints( const char* spoints ) {
  vector<cv::Point2f> points;

  int n = 0;
  char *p = (char*)spoints-1;
  while( true ) {
    p++;
    double x, y;
    if( sscanf( p, "%lf,%lf", &x, &y ) != 2 )
      break;
    points.push_back(cv::Point2f(x,y));
    n++;
    if( (p = strchr(p,' ')) == NULL )
      break;
  }

  return points;
}

/**
 * Parses a string of pairs of coordinates (x1,y1 [x2,y2 ...]) into an array.
 *
 * @param spoints  String containing coordinate pairs.
 * @return         Array of (x,y) coordinates.
 */
vector<cv::Point2f> PageXML::stringToPoints( string spoints ) {
  return stringToPoints( spoints.c_str() );
}

/**
 * Gets the minimum and maximum coordinate values for an array of points.
 *
 * @param points     The vector of points to find the limits.
 * @param xmin       Minimum x value.
 * @param xmax       Maximum x value.
 * @param ymin       Minimum y value.
 * @param ymax       Maximum y value.
 */
void PageXML::pointsLimits( vector<cv::Point2f>& points, double& xmin, double& xmax, double& ymin, double& ymax ) {
  if( points.size() == 0 )
    return;

  xmin = xmax = points[0].x;
  ymin = ymax = points[0].y;

  for( auto&& point : points ) {
    if( xmin > point.x ) xmin = point.x;
    if( xmax < point.x ) xmax = point.x;
    if( ymin > point.y ) ymin = point.y;
    if( ymax < point.y ) ymax = point.y;
  }

  return;
}

/**
 * Generates a vector of 4 points that define the bounding box for a given vector of points.
 *
 * @param points     The vector of points to find the limits.
 * @param bbox       The 4 points defining the bounding box.
 */
void PageXML::pointsBBox( vector<cv::Point2f>& points, vector<cv::Point2f>& bbox ) {
  bbox.clear();
  if( points.size() == 0 )
    return;

  double xmin;
  double xmax;
  double ymin;
  double ymax;

  pointsLimits( points, xmin, xmax, ymin, ymax );

  bbox.push_back( cv::Point2f(xmin,ymin) );
  bbox.push_back( cv::Point2f(xmax,ymin) );
  bbox.push_back( cv::Point2f(xmax,ymax) );
  bbox.push_back( cv::Point2f(xmin,ymax) );

  return;
}

/**
 * Determines whether a vector of points defines a bounding box.
 *
 * @param points   The vector of points to find the limits.
 * @return         True if bounding box, otherwise false.
 */
bool PageXML::isBBox( const vector<cv::Point2f>& points ) {
  if( points.size() == 4 &&
      points[0].x == points[3].x &&
      points[0].y == points[1].y &&
      points[1].x == points[2].x &&
      points[2].y == points[3].y )
    return true;
  return false;
}

/**
 * Cronvers a vector of points to a string in format "x1,y1 x2,y2 ...".
 *
 * @param points   Vector of points.
 * @param rounded  Whether to round values.
 * @return         String representation of the points.
 */
string PageXML::pointsToString( vector<cv::Point2f> points, bool rounded ) {
  char val[64];
  string str("");
  for( size_t n=0; n<points.size(); n++ ) {
    sprintf( val, rounded ? "%.0f,%.0f" : "%g,%g", points[n].x, points[n].y );
    str += ( n == 0 ? "" : " " ) + string(val);
  }
  return str;
}

/**
 * Cronvers a vector of points to a string in format "x1,y1 x2,y2 ...".
 *
 * @param points  Vector of points.
 * @return        String representation of the points.
 */
string PageXML::pointsToString( vector<cv::Point> points ) {
  char val[32];
  string str("");
  for( size_t n=0; n<points.size(); n++ ) {
    sprintf( val, "%d,%d", points[n].x, points[n].y );
    str += ( n == 0 ? "" : " " ) + string(val);
  }
  return str;
}

/**
 * Returns number of matched nodes for a given xpath.
 *
 * @param xpath  Selector expression.
 * @param node   XML node for context, set to NULL for root node.
 * @return       Number of matched nodes.
 */
int PageXML::count( const char* xpath, xmlNodePt basenode ) {
  return select( xpath, basenode ).size();
}

/**
 * Returns number of matched nodes for a given xpath.
 *
 * @param xpath  Selector expression.
 * @param node   XML node for context, set to NULL for root node.
 * @return       Number of matched nodes.
 */
int PageXML::count( string xpath, xmlNodePt basenode ) {
  return select( xpath.c_str(), basenode ).size();
}

/**
 * Selects nodes given an xpath.
 *
 * @param xpath  Selector expression.
 * @param node   XML node for context, set to NULL for root node.
 * @return       Vector of matched nodes.
 */
vector<xmlNodePt> PageXML::select( const char* xpath, const xmlNodePt basenode ) {
  vector<xmlNodePt> matched;

#define __REUSE_CONTEXT__

  xmlXPathContextPtr ncontext = context;
  if( basenode != NULL ) {
#ifndef __REUSE_CONTEXT__
    ncontext = xmlXPathNewContext(basenode->doc);
    if( ncontext == NULL ) {
      throw_runtime_error( "PageXML.select: unable create xpath context" );
      return matched;
    }
    if( xmlXPathRegisterNs( ncontext, (xmlChar*)"_", (xmlChar*)pagens ) != 0 ) {
      throw_runtime_error( "PageXML.select: unable to register namespace" );
      return matched;
    }
#endif
    ncontext->node = (xmlNodePtr)basenode;
  }

  xmlXPathObjectPtr xsel = xmlXPathEvalExpression( (xmlChar*)xpath, ncontext );
#ifdef __REUSE_CONTEXT__
  if( basenode != NULL )
    ncontext->node = rootnode;
#else
  if( ncontext != context )
    xmlXPathFreeContext(ncontext);
#endif

  if( xsel == NULL ) {
    throw_runtime_error( "PageXML.select: xpath expression failed: %s", xpath );
    return matched;
  }
  else {
    if( ! xmlXPathNodeSetIsEmpty(xsel->nodesetval) )
      for( int n=0; n<xsel->nodesetval->nodeNr; n++ )
        matched.push_back( xsel->nodesetval->nodeTab[n] );
    xmlXPathFreeObject(xsel);
  }

  return matched;
}

/**
 * Selects nodes given an xpath.
 *
 * @param xpath  Selector expression.
 * @param node   XML node for context, set to NULL for root node.
 * @return       Vector of matched nodes.
 */
vector<xmlNodePt> PageXML::select( string xpath, const xmlNodePt node ) {
  return select( xpath.c_str(), node );
}

/**
 * Selects the n-th node that matches an xpath.
 *
 * @param xpath  Selector expression.
 * @param num    Element number (0-indexed).
 * @param node   XML node for context, set to NULL for root node.
 * @return       Matched node.
 */
xmlNodePt PageXML::selectNth( const char* xpath, unsigned num, const xmlNodePt node ) {
  vector<xmlNodePt> matches = select( xpath, node );
  return matches.size() > num ? select( xpath, node )[num] : NULL;
}

/**
 * Selects the n-th node that matches an xpath.
 *
 * @param xpath  Selector expression.
 * @param num    Element number (0-indexed).
 * @param node   XML node for context, set to NULL for root node.
 * @return       Matched node.
 */
xmlNodePt PageXML::selectNth( string xpath, unsigned num, const xmlNodePt node ) {
  return selectNth( xpath.c_str(), num, node );
}

/**
 * Selects an element with a given ID.
 *
 * @param id     ID of element to select.
 * @param node   XML node for context, set to NULL for root node.
 * @return       Matched node.
 */
xmlNodePt PageXML::selectByID( const char* id, const xmlNodePt node ) {
  vector<xmlNodePt> sel = select( (string(".//*[@id='")+id+"']").c_str(), node );
  return sel.size() == 0 ? NULL : sel[0];
}

/**
 * Selects closest node of a given name.
 */
xmlNodePt PageXML::closest( const char* name, xmlNodePt node ) {
  return selectNth( string("ancestor-or-self::*[local-name()='")+name+"']", 0, node );
}

/**
 * Returns the parent of a node.
 *
 * @param node   XML node.
 * @return       Parent node.
 */
xmlNodePt PageXML::parent( const xmlNodePt node ) {
  return node->parent;
}

/**
 * Checks if node is of given name.
 *
 * @param node  XML node.
 * @param name  String with name to match against.
 * @return      True if name matches, otherwise false.
 */
bool PageXML::nodeIs( xmlNodePt node, const char* name ) {
  return ! node || xmlStrcmp( node->name, (const xmlChar*)name ) ? false : true;
}

/**
 * Gets the name of the given node.
 *
 * @param node  XML node.
 * @return      String with the name.
 */
std::string PageXML::getNodeName( xmlNodePt node, xmlNodePt base_node ) {
  string nodename = getAttr( node, "id" );
  if( nodename.empty() ) {
    throw_runtime_error( "PageXML.getNodeName: expected element to include id attribute" );
    return nodename;
  }
  
  if( base_node != NULL )
    nodename = getValue(base_node) + "." + nodename;
  else {
    xmlNodePt page = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
    nodename = pagesImageBase[getPageNumber(page)] + "." + nodename;
  }

  return nodename;
}

#if defined (__PAGEXML_LEPT__) || defined (__PAGEXML_MAGICK__) || defined (__PAGEXML_CVIMG__)

/**
 * Crops images using its Coords polygon, regions outside the polygon are set to transparent.
 *
 * @param xpath          Selector for polygons to crop.
 * @param margin         Margins, if >1.0 pixels, otherwise percentage of maximum of crop width and height.
 * @param opaque_coords  Whether to include an alpha channel with the polygon interior in opaque.
 * @param transp_xpath   Selector for semi-transparent elements.
 * @return               An std::vector containing NamedImage objects of the cropped images.
 */
vector<NamedImage> PageXML::crop( const char* xpath, cv::Point2f* margin, bool opaque_coords, const char* transp_xpath, const char* base_xpath ) {
  vector<NamedImage> images;

  vector<xmlNodePt> elems_coords = select( xpath );
  if( elems_coords.size() == 0 )
    return images;

  xmlNodePt prevPage = NULL;
  //string imageBase;
  unsigned int width = 0;
  unsigned int height = 0;
#if defined (__PAGEXML_LEPT__)
  PageImage pageImage = NULL;
#else
  PageImage pageImage;
#endif

  xmlNodePt base_node = NULL;
  if( base_xpath != NULL ) {
    base_node = selectNth( base_xpath, 0 );
    if( base_node == NULL ) {
      throw_runtime_error( "PageXML.crop: base xpath did not match any nodes: xpath=%s", base_xpath );
      return images;
    }
  }
  // @todo Allow base_xpath to be relative to node, e.g. to select a different property for each page, region, etc.

  for( int n=0; n<(int)elems_coords.size(); n++ ) {
    xmlNodePt node = elems_coords[n];

    if( ! nodeIs( node, "Coords") ) {
      throw_runtime_error( "PageXML.crop: expected xpath to match only Coords elements: match=%d xpath=%s", n+1, xpath );
      return images;
    }

    xmlNodePt page = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
    if( prevPage != page ) {
      prevPage = page;
      int pagenum = getPageNumber(page);
      //imageBase = pagesImageBase[pagenum];
      width = getPageWidth(page);
      height = getPageHeight(page);
      #if defined (__PAGEXML_LEPT__)
        if( pagesImage[pagenum] == NULL )
      #elif defined (__PAGEXML_MAGICK__)
        if( pagesImage[pagenum].columns() == 0 )
      #elif defined (__PAGEXML_CVIMG__)
        if( ! pagesImage[pagenum].data )
      #endif
          loadImage(pagenum);
        pageImage = pagesImage[pagenum];
    }

    /// Get parent node id ///
    string sampid = getAttr( node->parent, "id" );
    if( sampid.empty() ) {
      throw_runtime_error( "PageXML.crop: expected parent element to include id attribute: match=%d xpath=%s", n+1, xpath );
      return images;
    }

    /// Construct sample name ///
    //string sampname = imageBase + "." + sampid;
    std::string sampname = getNodeName( node->parent, base_node );

    /// Get coords points ///
    string spoints = getAttr( node, "points" );
    if( spoints.empty() ) {
      throw_runtime_error( "PageXML.crop: expected a points attribute in Coords element: id=%s", sampid.c_str() );
      return images;
    }
    vector<cv::Point2f> coords = stringToPoints( spoints );

    /// Get crop window parameters ///
    double xmin=0, xmax=0, ymin=0, ymax=0;
    pointsLimits( coords, xmin, xmax, ymin, ymax );
    size_t cropW = (size_t)(ceil(xmax)-floor(xmin)+1);
    size_t cropH = (size_t)(ceil(ymax)-floor(ymin)+1);
    int cropX = (int)floor(xmin);
    int cropY = (int)floor(ymin);

    /// Add margin to bounding box ///
    if( margin != NULL ) {
      int maxWH = cropW > cropH ? cropW : cropH;
      int ocropX = cropX;
      int ocropY = cropY;
      cropX -= margin[0].x < 1.0 ? maxWH*margin[0].x : margin[0].x;
      cropY -= margin[0].y < 1.0 ? maxWH*margin[0].y : margin[0].y;
      cropX = cropX < 0 ? 0 : cropX;
      cropY = cropY < 0 ? 0 : cropY;
      cropW += ocropX - cropX;
      cropH += ocropY - cropY;
      cropW += margin[1].x < 1.0 ? maxWH*margin[1].x : margin[1].x;
      cropH += margin[1].y < 1.0 ? maxWH*margin[1].y : margin[1].y;
      cropW = cropX+cropW-1 >= width ? width-cropX-1 : cropW;
      cropH = cropY+cropH-1 >= height ? height-cropY-1 : cropH;
    }

    /// Crop image ///
#if defined (__PAGEXML_LEPT__)
    BOX* box = boxCreate(cropX, cropY, cropW, cropH);
    Pix* cropimg = pixClipRectangle(pageImage, box, NULL);
    boxDestroy(&box);
#elif defined (__PAGEXML_MAGICK__)
    Magick::Image cropimg = pageImage;
    try {
      cropimg.crop( Magick::Geometry(cropW,cropH,cropX,cropY) );
      // @todo If crop partially outside image it will not fail, but fpgram will be wrong?
    }
    catch( exception& error ) {
      fprintf( stderr, "PageXML.crop: error (%s): %s\n", sampname.c_str(), error.what() );
      continue;
    }
#elif defined (__PAGEXML_CVIMG__)
    cv::Rect roi;
    roi.x = cropX;
    roi.y = cropY;
    roi.width = cropW;
    roi.height = cropH;
    cv::Mat cropimg = pageImage(roi);
#endif

    if( opaque_coords /*&& ! isBBox( coords )*/ ) {
#if defined (__PAGEXML_LEPT__)
      if( transp_xpath != NULL ) {
        throw_runtime_error( "PageXML.crop: transp_xpath not implemented for __PAGEXML_LEPT__" );
        return images;
      }
      throw_runtime_error( "PageXML.crop: opaque_coords not implemented for __PAGEXML_LEPT__" );
      return images;

#elif defined (__PAGEXML_MAGICK__)
      /// Subtract crop window offset ///
      for( auto&& coord : coords ) {
        coord.x -= cropX;
        coord.y -= cropY;
      }

      /// Add transparency layer ///
      list<Magick::Drawable> drawList;
      drawList.push_back( Magick::DrawableStrokeColor(opaque) );
      drawList.push_back( Magick::DrawableFillColor(opaque) );
      drawList.push_back( Magick::DrawableFillRule(Magick::NonZeroRule) );
      drawList.push_back( Magick::DrawableStrokeAntialias(false) );
      drawList.push_back( Magick::DrawablePolygon(cvToMagick(coords)) );
      Magick::Image mask( Magick::Geometry(cropW,cropH), transparent );
      mask.draw(drawList);
      cropimg.draw( Magick::DrawableCompositeImage(0,0,0,0,mask,Magick::CopyOpacityCompositeOp) );

      if( transp_xpath != NULL ) {
        throw_runtime_error( "PageXML.crop: transp_xpath not implemented for __PAGEXML_MAGICK__" );
        return images;
      }

#elif defined (__PAGEXML_CVIMG__)
      /// Subtract crop window offset and round points ///
      std::vector<cv::Point> rcoods;
      std::vector<std::vector<cv::Point> > polys;
      for( auto&& coord : coords )
        rcoods.push_back( cv::Point( round(coord.x-cropX), round(coord.y-cropY) ) );
      polys.push_back(rcoods);

      /// Draw opaque polygon for Coords ///
      cv::Mat wmask( cropimg.size(), CV_MAKE_TYPE(cropimg.type(),cropimg.channels()+1), cv::Scalar(0,0,0,0) );
      cv::fillPoly( wmask, polys, cv::Scalar(0,0,0,255) );

      /// Draw semi-transparent polygons according to xpath ///
      if( transp_xpath != NULL ) {
        vector<xmlNodePt> child_coords = select( transp_xpath, node );

        polys.clear();
        for( int m=0; m<(int)child_coords.size(); m++ ) {
          xmlNodePt childnode = child_coords[m];

          string childid = getAttr( childnode->parent, "id" );

          spoints = getAttr( childnode, "points" );
          if( spoints.empty() ) {
            throw_runtime_error( "PageXML.crop: expected a points attribute in Coords element: id=%s", childid.c_str() );
            return images;
          }
          coords = stringToPoints( spoints );

          std::vector<cv::Point> rcoods;
          for( auto&& coord : coords )
            rcoods.push_back( cv::Point( round(coord.x-cropX), round(coord.y-cropY) ) );
          polys.push_back(rcoods);
        }
        cv::fillPoly( wmask, polys, cv::Scalar(0,0,0,128) );
      }

      /// Add alpha channel to image ///
      int from_to[] = { 0,0, 1,1, 2,2 };
      cv::mixChannels( &cropimg, 1, &wmask, 1, from_to, cropimg.channels() );
      cropimg = wmask;
#endif
    }

    double rotation = std::numeric_limits<double>::quiet_NaN();
    if ( nodeIs( node->parent, "TextLine" ) )
      rotation = getBaselineOrientation( node->parent )*180.0/M_PI;
    if ( std::isnan(rotation) )
      rotation = getRotation(node->parent);

    /// Append crop and related data to list ///
    NamedImage namedimage(
      sampid,
      sampname,
      rotation,
      getReadingDirection(node->parent),
      cropX,
      cropY,
      cropimg,
      node );

    images.push_back(namedimage);
  }

  return images;
}

#endif

/**
 * Retrieves a node value.
 *
 * @param node       Node element.
 * @return           String with the node value.
 */
std::string PageXML::getValue( xmlNodePt node ) {
  if( node == NULL )
    throw_runtime_error( "PageXML.getValue: received NULL pointer" );
  xmlChar* val = xmlNodeGetContent(node);
  std::string text = (char*)val;
  xmlFree(val);
  return text;
}

/**
 * Gets an attribute value from an xml node.
 *
 * @param node   XML node.
 * @param name   Attribute name.
 * @param value  String to set the value.
 * @return       True if attribute found, otherwise false.
*/
string PageXML::getAttr( const xmlNodePt node, const char* name ) {
  string value("");
  if( node == NULL )
    return value;

  xmlChar* attr = xmlGetProp( node, (xmlChar*)name );
  if( attr == NULL )
    return value;
  value = string((char*)attr);
  xmlFree(attr);

  return value;
}

/**
 * Gets an attribute value for a given xpath.
 *
 * @param xpath  Selector for the element to get the attribute.
 * @param name   Attribute name.
 * @param value  String to set the value.
 * @return       True if attribute found, otherwise false.
*/
string PageXML::getAttr( const char* xpath, const char* name ) {
  vector<xmlNodePt> xsel = select( xpath );
  if( xsel.size() == 0 )
    return string("");

  return getAttr( xsel[0], name );
}

/**
 * Gets an attribute value for a given xpath.
 *
 * @param xpath  Selector for the element to get the attribute.
 * @param name   Attribute name.
 * @param value  String to set the value.
 * @return       True if attribute found, otherwise false.
*/
string PageXML::getAttr( const string xpath, const string name ) {
  vector<xmlNodePt> xsel = select( xpath.c_str() );
  if( xsel.size() == 0 )
    return string("");

  return getAttr( xsel[0], name.c_str() );
}

/**
 * Adds or modifies (if already exists) an attribute for a given list of nodes.
 *
 * @param nodes  Vector of nodes to set the attribute.
 * @param name   Attribute name.
 * @param value  Attribute value.
 * @return       Number of elements modified.
 */
int PageXML::setAttr( vector<xmlNodePt> nodes, const char* name, const char* value ) {
  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
    xmlAttrPtr attr = xmlHasProp( nodes[n], (xmlChar*)name ) ?
      xmlSetProp( nodes[n], (xmlChar*)name, (xmlChar*)value ) :
      xmlNewProp( nodes[n], (xmlChar*)name, (xmlChar*)value ) ;
    if( ! attr ) {
      throw_runtime_error( "PageXML.setAttr: problems setting attribute: name=%s", name );
      return 0;
    }
  }

  return (int)nodes.size();
}

/**
 * Adds or modifies (if already exists) an attribute for a given node.
 *
 * @param node   Node to set the attribute.
 * @param name   Attribute name.
 * @param value  Attribute value.
 * @return       Number of elements modified.
 */
int PageXML::setAttr( const xmlNodePt node, const char* name, const char* value ) {
  return setAttr( vector<xmlNodePt>{(xmlNodePtr)node}, name, value );
}

/**
 * Adds or modifies (if already exists) an attribute for a given xpath.
 *
 * @param xpath  Selector for the element(s) to set the attribute.
 * @param name   Attribute name.
 * @param value  Attribute value.
 * @return       Number of elements modified.
 */
int PageXML::setAttr( const char* xpath, const char* name, const char* value ) {
  return setAttr( select(xpath), name, value );
}

/**
 * Adds or modifies (if already exists) an attribute for a given xpath.
 *
 * @param xpath  Selector for the element(s) to set the attribute.
 * @param name   Attribute name.
 * @param value  Attribute value.
 * @return       Number of elements modified.
 */
int PageXML::setAttr( const string xpath, const string name, const string value ) {
  return setAttr( select(xpath.c_str()), name.c_str(), value.c_str() );
}

/**
 * Inserts an element relative to a given node.
 *
 * @param elem   Element to insert.
 * @param node   Reference element for insertion.
 * @param itype  Type of insertion.
 * @return       Pointer to inserted element.
 */
xmlNodePt PageXML::insertElem( xmlNodePt elem, const xmlNodePt node, PAGEXML_INSERT itype ) {
  assert( elem != NULL );

  xmlNodePt sel = NULL;
  switch( itype ) {
    case PAGEXML_INSERT_APPEND:
      elem = xmlAddChild((xmlNodePtr)node,elem);
      break;
    case PAGEXML_INSERT_PREPEND:
      sel = selectNth("*",0,node);
      if( sel )
        elem = xmlAddPrevSibling(sel,elem);
      else
        elem = xmlAddChild((xmlNodePtr)node,elem);
      break;
    case PAGEXML_INSERT_NEXTSIB:
      elem = xmlAddNextSibling((xmlNodePtr)node,elem);
      break;
    case PAGEXML_INSERT_PREVSIB:
      elem = xmlAddPrevSibling((xmlNodePtr)node,elem);
      break;
  }

  return elem;
}


/**
 * Creates a new element and adds it relative to a given node.
 *
 * @param name   Name of element to create.
 * @param id     ID attribute for element.
 * @param node   Reference element for insertion.
 * @param itype  Type of insertion.
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::addElem( const char* name, const char* id, const xmlNodePt node, PAGEXML_INSERT itype, bool checkid ) {
  xmlNodePt elem = xmlNewNode( rpagens, (xmlChar*)name );
  if( ! elem ) {
    throw_runtime_error( "PageXML.addElem: problems creating new element: name=%s", name );
    return NULL;
  }
  if( id != NULL ) {
    if( checkid ) {
      vector<xmlNodePt> idsel = select( (string("//*[@id='")+id+"']").c_str() );
      if( idsel.size() > 0 ) {
        throw_runtime_error( "PageXML.addElem: id already exists: id=%s", id );
        return NULL;
      }
    }
    xmlNewProp( elem, (xmlChar*)"id", (xmlChar*)id );
  }

  return insertElem( elem, node, itype );

/*   xmlNodePt sel = NULL;
  switch( itype ) {
    case PAGEXML_INSERT_APPEND:
      elem = xmlAddChild((xmlNodePtr)node,elem);
      break;
    case PAGEXML_INSERT_PREPEND:
      sel = selectNth("*",0,node);
      if( sel )
        elem = xmlAddPrevSibling(sel,elem);
      else
        elem = xmlAddChild((xmlNodePtr)node,elem);
      break;
    case PAGEXML_INSERT_NEXTSIB:
      elem = xmlAddNextSibling((xmlNodePtr)node,elem);
      break;
    case PAGEXML_INSERT_PREVSIB:
      elem = xmlAddPrevSibling((xmlNodePtr)node,elem);
      break;
  }

  return elem; */
}

/**
 * Creates a new element and adds it relative to a given xpath.
 *
 * @param name   Name of element to create.
 * @param id     ID attribute for element.
 * @param xpath  Selector for insertion.
 * @param itype  Type of insertion.
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::addElem( const char* name, const char* id, const char* xpath, PAGEXML_INSERT itype, bool checkid ) {
  vector<xmlNodePt> target = select( xpath );
  if( target.size() == 0 ) {
    throw_runtime_error( "PageXML.addElem: unmatched target: xpath=%s", xpath );
    return NULL;
  }

  return addElem( name, id, target[0], itype, checkid );
}

/**
 * Creates a new element and adds it relative to a given xpath.
 *
 * @param name   Name of element to create.
 * @param id     ID attribute for element.
 * @param xpath  Selector for insertion.
 * @param itype  Type of insertion.
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::addElem( const string name, const string id, const string xpath, PAGEXML_INSERT itype, bool checkid ) {
  vector<xmlNodePt> target = select( xpath );
  if( target.size() == 0 ) {
    throw_runtime_error( "PageXML.addElem: unmatched target: xpath=%s", xpath.c_str() );
    return NULL;
  }

  return addElem( name.c_str(), id.c_str(), target[0], itype, checkid );
}

/**
 * Removes the given element.
 *
 * @param node   Element.
 */
void PageXML::rmElem( const xmlNodePt node ) {
  xmlUnlinkNode((xmlNodePtr)node);
  xmlFreeNode((xmlNodePtr)node);
}

/**
 * Removes the elements given in a vector.
 *
 * @param nodes  Vector of elements.
 * @return       Number of elements removed.
 */
int PageXML::rmElems( const vector<xmlNodePt>& nodes ) {
  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
    xmlUnlinkNode((xmlNodePtr)nodes[n]);
    xmlFreeNode((xmlNodePtr)nodes[n]);
  }

  return (int)nodes.size();
}

/**
 * Remove the elements that match a given xpath.
 *
 * @param xpath  Selector for elements to remove.
 * @param node   Base node for element selection.
 * @return       Number of elements removed.
 */
int PageXML::rmElems( const char* xpath, xmlNodePt basenode ) {
  return rmElems( select( xpath, basenode ) );
}

/**
 * Remove the elements that match a given xpath.
 *
 * @param xpath    Selector for elements to remove.
 * @param basenode Base node for element selection.
 * @return         Number of elements removed.
 */
int PageXML::rmElems( const string xpath, xmlNodePt basenode ) {
  return rmElems( select( xpath.c_str(), basenode ) );
}

/**
 * Unlink an element and add it relative to a given node.
 *
 * @param elem   Element to move.
 * @param node   Reference element for insertion.
 * @param itype  Type of insertion.
 * @return       Pointer to moved element.
 */
xmlNodePt PageXML::moveElem( xmlNodePt elem, const xmlNodePt node, PAGEXML_INSERT itype ) {
  assert( elem != NULL );
  xmlUnlinkNode(elem);
  return insertElem( elem, node, itype );
}

/**
 * Unlink elements and add them relative to a given node.
 *
 * @param elem   Element to move.
 * @param node   Reference element for insertion.
 * @param itype  Type of insertion.
 * @return       Pointer to moved element.
 */
int PageXML::moveElems( const std::vector<xmlNodePt>& elems, const xmlNodePt node, PAGEXML_INSERT itype ) {
  int moves = 0;
  switch( itype ) {
    case PAGEXML_INSERT_APPEND:
    case PAGEXML_INSERT_PREVSIB:
      for( int n=0; n<(int)elems.size(); n++ )
        if( moveElem(elems[n],node,itype) != NULL )
          moves++;
      break;
    case PAGEXML_INSERT_PREPEND:
    case PAGEXML_INSERT_NEXTSIB:
      for( int n=(int)elems.size()-1; n>=0; n-- )
        if( moveElem(elems[n],node,itype) != NULL )
          moves++;
      break;
  }

  return moves;
}

/**
 * Sets the rotation angle to a TextRegion node.
 *
 * @param node       Node of the TextRegion element.
 * @param rotation   Rotation angle to set.
 */
void PageXML::setRotation( const xmlNodePt node, const float rotation ) {
  if( ! xmlStrcmp( node->name, (const xmlChar*)"TextRegion") ) {
    char rot[64];
    snprintf( rot, sizeof rot, "%g", rotation );
    if( rotation )
      setAttr( node, "readingOrientation", rot );
    else
      rmElems( select( "@readingOrientation", node ) );
  }
  else {
    throw_runtime_error( "PageXML.setRotation: only possible for TextRegion" );
    return;
  }
}

/**
 * Sets the reading direction to a TextRegion node.
 *
 * @param node       Node of the TextRegion element.
 * @param direction  Direction to set.
 */
void PageXML::setReadingDirection( const xmlNodePt node, PAGEXML_READ_DIRECTION direction ) {
  if( ! xmlStrcmp( node->name, (const xmlChar*)"TextRegion") ) {
    if( direction == PAGEXML_READ_DIRECTION_RTL )
      setAttr( node, "readingDirection", "right-to-left" );
    else if( direction == PAGEXML_READ_DIRECTION_TTB )
      setAttr( node, "readingDirection", "top-to-bottom" );
    else if( direction == PAGEXML_READ_DIRECTION_BTT )
      setAttr( node, "readingDirection", "bottom-to-top" );
    else if( direction == PAGEXML_READ_DIRECTION_LTR )
      //setAttr( node, "readingDirection", "left-to-right" );
      rmElems( select( "@readingDirection", node ) );
  }
  else {
    throw_runtime_error( "PageXML.setReadingDirection: only possible for TextRegion" );
    return;
  }
}

/**
 * Projects points onto a line defined by a direction and y-offset
 */
std::vector<double> static project_2d_to_1d( std::vector<cv::Point2f> points, cv::Point2f axis, double yoffset = 0.0 ) {
  axis *= 1.0/cv::norm(axis);
  std::vector<double> proj(points.size());
  for ( int n=0; n<(int)points.size(); n++ )
    proj[n] = points[n].x*axis.x + (points[n].y-yoffset)*axis.y;
  return proj;
}

/**
 * Computes the difference between two angles [-PI,PI] accounting for the discontinuity
 */
double static inline angleDiff( double a1, double a2 ) {
  double a = a1 - a2;
  a += (a>M_PI) ? -2*M_PI : (a<-M_PI) ? 2*M_PI : 0;
  return a;
}

/**
 * Computes the 1D intersection
 */
double static inline intersection_1d( double& a1, double& a2, double& b1, double& b2 ) {
  double tmp;
  if ( a1 > a2 ) {
    tmp = a1;
    a1 = a2;
    a2 = tmp;
  }
  if ( b1 > b2 ) {
    tmp = b1;
    b1 = b2;
    b2 = tmp;
  }
  return std::max(0.0, std::min(a2, b2) - std::max(a1, b1));
}

/**
 * Computes the 1D intersection over union
 */
double static inline IoU_1d( double a1, double a2, double b1, double b2 ) {
  double isect = intersection_1d(a1,a2,b1,b2);
  return isect == 0.0 ? 0.0 : isect/((a2-a1)+(b2-b1));
}

/**
 * Gets the (average) baseline orientation angle in radians of a given text line.
 *
 * @param elem   Node of the TextLine element.
 * @return       The orientation angle in radians, NaN if unset.
 */
double PageXML::getBaselineOrientation( xmlNodePt elem ) {
  if ( ! nodeIs( elem, "TextLine" ) ) {
    throw_runtime_error( "PageXML.getBaselineOrientation: node is required to be a TextLine" );
    return std::numeric_limits<double>::quiet_NaN();
  }
  std::vector<cv::Point2f> points = getPoints( elem, "_:Baseline" );
  return getBaselineOrientation(points);
}

/**
 * Gets the (average) baseline orientation angle in radians of a given baseline.
 *
 * @param points  Baseline points.
 * @return        The orientation angle in radians, NaN if unset.
 */
double PageXML::getBaselineOrientation( std::vector<cv::Point2f> points ) {
  if ( points.size() == 0 )
    return std::numeric_limits<double>::quiet_NaN();

  double avgAngle = 0.0;
  double totlgth = 0.0;
  double angle1st = 0.0;

  for ( int n = 1; n < (int)points.size(); n++ ) {
    double lgth = cv::norm(points[n]-points[n-1]);
    totlgth += lgth;
    double angle = -atan2( points[n].y-points[n-1].y, points[n].x-points[n-1].x );
    if ( n == 1 ) {
      angle1st = angle;
      avgAngle += lgth*angle;
    }
    else {
      avgAngle += lgth*(angle1st+angleDiff(angle,angle1st));
    }
  }

  return avgAngle/totlgth;
}

/**
 * Gets the baseline length.
 *
 * @param points  Baseline points.
 * @return        The orientation angle in radians, NaN if unset.
 */
double PageXML::getBaselineLength( std::vector<cv::Point2f> points ) {
  double totlgth = 0.0;
  for ( int n = 1; n < (int)points.size(); n++ )
    totlgth += cv::norm(points[n]-points[n-1]);
  return totlgth;
}

/**
 * Retrieves the rotation angle for a given TextLine or TextRegion node.
 *
 * @param elem   Node of the TextLine or TextRegion element.
 * @return       The rotation angle in degrees, 0 if unset.
 */
double PageXML::getRotation( const xmlNodePt elem ) {
  double rotation = 0.0;
  if( elem == NULL )
    return rotation;

  xmlNodePt node = (xmlNodePt)elem;

  /// If TextLine try to get rotation from custom attribute ///
  if( ! xmlStrcmp( node->name, (const xmlChar*)"TextLine") ) {
    if( ! xmlHasProp( node, (xmlChar*)"custom" ) )
      node = node->parent;
    else {
      xmlChar* attr = xmlGetProp( node, (xmlChar*)"custom" );
      cmatch base_match;
      if( regex_match((char*)attr,base_match,reRotation) )
        rotation = stod(base_match[1].str());
      else
        node = node->parent;
      xmlFree(attr);
    }
  }
  /// Otherwise try to get rotation from readingOrientation attribute ///
  if( xmlHasProp( node, (xmlChar*)"readingOrientation" ) ) {
    xmlChar* attr = xmlGetProp( node, (xmlChar*)"readingOrientation" );
    rotation = stod((char*)attr);
    xmlFree(attr);
  }

  return rotation;
}

/**
 * Retrieves the reading direction for a given TextLine or TextRegion node.
 *
 * @param elem   Node of the TextLine or TextRegion element.
 * @return       The reading direction, PAGEXML_READ_DIRECTION_LTR if unset.
 */
int PageXML::getReadingDirection( const xmlNodePt elem ) {
  int direction = PAGEXML_READ_DIRECTION_LTR;
  if( elem == NULL )
    return direction;

  xmlNodePt node = (xmlNodePt)elem;

  /// If TextLine try to get direction from custom attribute ///
  if( ! xmlStrcmp( node->name, (const xmlChar*)"TextLine") ) {
    if( ! xmlHasProp( node, (xmlChar*)"custom" ) )
      node = node->parent;
    else {
      xmlChar* attr = xmlGetProp( node, (xmlChar*)"custom" );
      cmatch base_match;
      if( regex_match((char*)attr,base_match,reDirection) ) {
        if( ! strcmp(base_match[1].str().c_str(),"ltr") )
          direction = PAGEXML_READ_DIRECTION_LTR;
        else if( ! strcmp(base_match[1].str().c_str(),"rtl") )
          direction = PAGEXML_READ_DIRECTION_RTL;
        else if( ! strcmp(base_match[1].str().c_str(),"ttb") )
          direction = PAGEXML_READ_DIRECTION_TTB;
        else if( ! strcmp(base_match[1].str().c_str(),"btt") )
          direction = PAGEXML_READ_DIRECTION_BTT;
      }
      else
        node = node->parent;
      xmlFree(attr);
    }
  }
  /// Otherwise try to get direction from readingDirection attribute ///
  if( xmlHasProp( node, (xmlChar*)"readingDirection" ) ) {
    char* attr = (char*)xmlGetProp( node, (xmlChar*)"readingDirection" );
    if( ! strcmp(attr,"left-to-right") )
      direction = PAGEXML_READ_DIRECTION_LTR;
    else if( ! strcmp(attr,"right-to-left") )
      direction = PAGEXML_READ_DIRECTION_RTL;
    else if( ! strcmp(attr,"top-to-bottom") )
      direction = PAGEXML_READ_DIRECTION_TTB;
    else if( ! strcmp(attr,"bottom-to-top") )
      direction = PAGEXML_READ_DIRECTION_BTT;
    xmlFree(attr);
  }

  return direction;
}

/**
 * Retrieves the x-height for a given TextLine node.
 *
 * @param node   Node of the TextLine element.
 * @return       x-height>0 on success, -1 if unset.
 */
float PageXML::getXheight( const xmlNodePt node ) {
  float xheight = -1;

  if( node != NULL &&
      xmlHasProp( node, (xmlChar*)"custom" ) ) {
    xmlChar* custom = xmlGetProp( node, (xmlChar*)"custom" );
    cmatch base_match;
    if( regex_match((char*)custom,base_match,reXheight) )
      xheight = stof(base_match[1].str());
    xmlFree(custom);
  }

  return xheight;
}

/**
 * Retrieves the x-height for a given TextLine id.
 *
 * @param id     Identifier of the TextLine.
 * @return       x-height>0 on success, -1 if unset.
 */
float PageXML::getXheight( const char* id ) {
  vector<xmlNodePt> elem = select( string("//*[@id='")+id+"']" );
  return getXheight( elem.size() == 0 ? NULL : elem[0] );
}

/**
 * Retrieves the features parallelogram (Property[@key="fpgram"]/@value) for a given node.
 *
 * @param node   Base node.
 * @return       Reference to the points vector.
 */
vector<cv::Point2f> PageXML::getFpgram( const xmlNodePt node ) {
  vector<cv::Point2f> points;
  if( node == NULL )
    return points;

  vector<xmlNodePt> coords = select( "_:Property[@key='fpgram']", node );
  if( coords.size() == 0 )
    return points;

  string spoints = getAttr( coords[0], "value" );
  if( spoints.empty() )
    return points;

  points = stringToPoints( spoints.c_str() );
  if( points.size() != 4 ) {
    throw_runtime_error( "PageXML.getFpgram: expected property value to be four points" );
    return points;
  }

  return points;
}

/**
 * Retrieves and parses the Coords/@points for a given base node.
 *
 * @param node   Base node.
 * @return       Reference to the points vector.
 */
vector<cv::Point2f> PageXML::getPoints( const xmlNodePt node, const char* xpath ) {
  vector<cv::Point2f> points;
  if( node == NULL )
    return points;

  vector<xmlNodePt> coords = select( xpath, node );
  if( coords.size() == 0 )
    return points;

  string spoints = getAttr( coords[0], "points" );
  if( spoints.empty() )
    return points;

  return stringToPoints( spoints.c_str() );
}

/**
 * Retrieves and parses the Coords/@points for a given list of base nodes.
 *
 * @param nodes  Base nodes.
 * @return       Reference to the points vector.
 */
std::vector<std::vector<cv::Point2f> > PageXML::getPoints( const std::vector<xmlNodePt> nodes, const char* xpath ) {
  std::vector<std::vector<cv::Point2f> > points;
  for ( int n=0; n<(int)nodes.size(); n++ ) {
    std::vector<cv::Point2f> pts_n = getPoints( nodes[n], xpath );
    if ( pts_n.size() == 0 )
      return std::vector<std::vector<cv::Point2f> >();
    points.push_back(pts_n);
  }
  return points;
}

/**
 * Retrieves the concatenated TextEquivs for a given root node and xpath.
 *
 * @param node       Root node element.
 * @param xpath      Relative xpath to select the TextEquiv elements.
 * @param separator  String to add between TextEquivs.
 * @return           String with the concatenated TextEquivs.
 */
std::string PageXML::getTextEquiv( xmlNodePt node, const char* xpath, const char* separator ) {
  std::vector<xmlNodePt> nodes = select( std::string(xpath)+"/_:TextEquiv/_:Unicode", node );
  std::string text;
  for ( int n=0; n<(int)nodes.size(); n++ ) {
    xmlChar* t = xmlNodeGetContent(nodes[n]);
    text += std::string(n==0?"":separator) + (char*)t;
    xmlFree(t);
  }
  return text;
}

/**
 * Starts a process in the Page XML.
 */
void PageXML::processStart( const char* tool, const char* ref ) {
  if( tool == NULL || tool[0] == '\0' ) {
    throw_runtime_error( "PageXML.processStart: tool string is required" );
    return;
  }
  if( ref != NULL && ref[0] == '\0' ) {
    throw_runtime_error( "PageXML.processStart: ref if provided cannot be empty" );
    return;
  }

  process_started = chrono::high_resolution_clock::now();

  /// Add Process element ///
  process_running = addElem( "Process", NULL, "//_:Metadata" );
  if ( ! process_running ) {
    throw_runtime_error( "PageXML.processStart: problems creating element" );
    return;
  }

  /// Start time attribute ///
  time_t now;
  time(&now);
  char tstamp[sizeof "YYYY-MM-DDTHH:MM:SSZ"];
  strftime(tstamp, sizeof tstamp, "%FT%TZ", gmtime(&now));
  setAttr( process_running, "started", tstamp );

  /// Tool and ref attributes ///
  setAttr( process_running, "tool", tool );
  if ( ref != NULL )
    setAttr( process_running, "ref", ref );
}

/**
 * Ends the running process in the Page XML.
 */
void PageXML::processEnd() {
  if ( ! process_running )
    return;
  double duration = 1e-6*chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now()-process_started).count();
  char sduration[64];
  snprintf( sduration, sizeof sduration, "%g", duration );
  setAttr( process_running, "time", sduration );
  process_running = NULL;
  updateLastChange();
}

/**
 * Updates the last change time stamp.
 */
void PageXML::updateLastChange() {
  xmlNodePt lastchange = selectNth( "//_:LastChange", 0 );
  if( ! lastchange ) {
    throw_runtime_error( "PageXML.updateLastChange: unable to select node" );
    return;
  }
  rmElems( select( "text()", lastchange ) );
  time_t now;
  time(&now);
  char tstamp[sizeof "YYYY-MM-DDTHH:MM:SSZ"];
  strftime(tstamp, sizeof tstamp, "%FT%TZ", gmtime(&now));
  xmlNodePt text = xmlNewText( (xmlChar*)tstamp );
  if( ! text || ! xmlAddChild(lastchange,text) ) {
    throw_runtime_error( "PageXML.updateLastChange: problems updating time stamp" );
    return;
  }
}

/**
 * Retrieves a Property value.
 *
 * @param node       Node element.
 * @return           String with the property value.
 */
std::string PageXML::getPropertyValue( xmlNodePt node, const char* key ) {
  xmlNodePt prop = selectNth( std::string("_:Property[@key='")+key+"']/@value", 0, node );
  return prop == NULL ? std::string("") : getValue(prop);
}

/**
 * Sets a Property for a given xpath.
 *
 * @param node  The node of element to set the Property.
 * @param key   The key for the Property.
 * @param val   The optional value for the Property.
 * @param _conf Pointer to confidence value, NULL for no confidence.
 * @return      Pointer to created element.
 */
xmlNodePt PageXML::setProperty( xmlNodePt node, const char* key, const char* val, const double* _conf ) {
  rmElems( select( std::string("_:Property[@key=\"")+key+"\"]", node ) );

  std::vector<xmlNodePt> siblafter = select( "*[local-name()!='Property' and local-name()!='Metadata']", node );
  std::vector<xmlNodePt> props = select( "_:Property", node );

  xmlNodePt prop = NULL;
  if ( props.size() > 0 )
    prop = addElem( "Property", NULL, props[props.size()-1], PAGEXML_INSERT_NEXTSIB );
  else if ( siblafter.size() > 0 )
    prop = addElem( "Property", NULL, siblafter[0], PAGEXML_INSERT_PREVSIB );
  else
    prop = addElem( "Property", NULL, node );
  if ( ! prop ) {
    throw_runtime_error( "PageXML.setProperty: problems creating element" );
    return NULL;
  }

  if ( ! setAttr( prop, "key", key ) ) {
    rmElem( prop );
    throw_runtime_error( "PageXML.setProperty: problems setting key attribute" );
    return NULL;
  }
  if ( val != NULL && ! setAttr( prop, "value", val ) ) {
    rmElem( prop );
    throw_runtime_error( "PageXML.setProperty: problems setting value attribute" );
    return NULL;
  }

  if( _conf != NULL ) {
    char conf[64];
    snprintf( conf, sizeof conf, "%g", *_conf );
    if( ! xmlNewProp( prop, (xmlChar*)"conf", (xmlChar*)conf ) ) {
      rmElem( prop );
      throw_runtime_error( "PageXML.setProperty: problems setting conf attribute" );
      return NULL;
    }
  }

  return prop;
}

/**
 * Adds or modifies (if already exists) the TextEquiv for a given node.
 *
 * @param node   The node of element to set the TextEquiv.
 * @param text   The text string.
 * @param _conf  Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::setTextEquiv( xmlNodePt node, const char* text, const double* _conf ) {
  rmElems( select( "_:TextEquiv", node ) );

  xmlNodePt textequiv = addElem( "TextEquiv", NULL, node );

  xmlNodePt unicode = xmlNewTextChild( textequiv, NULL, (xmlChar*)"Unicode", (xmlChar*)text );
  if( ! unicode ) {
    throw_runtime_error( "PageXML.setTextEquiv: problems setting TextEquiv" );
    return NULL;
  }

  if( _conf != NULL ) {
    char conf[64];
    snprintf( conf, sizeof conf, "%g", *_conf );
    if( ! xmlNewProp( textequiv, (xmlChar*)"conf", (xmlChar*)conf ) ) {
      throw_runtime_error( "PageXML.setTextEquiv: problems setting conf attribute" );
      return NULL;
    }
  }

  return textequiv;
}

/**
 * Adds or modifies (if already exists) the TextEquiv for a given xpath.
 *
 * @param xpath  Selector for element to set the TextEquiv.
 * @param text   The text string.
 * @param _conf  Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::setTextEquiv( const char* xpath, const char* text, const double* _conf ) {
  vector<xmlNodePt> target = select( xpath );
  if( target.size() == 0 ) {
    throw_runtime_error( "PageXML.setTextEquiv: unmatched target: xpath=%s", xpath );
    return NULL;
  }

  return setTextEquiv( target[0], text, _conf );
}

/**
 * Adds or modifies (if already exists) the Coords for a given node.
 *
 * @param node   The node of element to set the Coords.
 * @param points Vector of x,y coordinates for the points attribute.
 * @param _conf  Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::setCoords( xmlNodePt node, const vector<cv::Point2f>& points, const double* _conf ) {
  rmElems( select( "_:Coords", node ) );

  xmlNodePt coords;
  vector<xmlNodePt> sel = select( "*[local-name()!='Property']", node );
  if( sel.size() > 0 )
    coords = addElem( "Coords", NULL, sel[0], PAGEXML_INSERT_PREVSIB );
  else
    coords = addElem( "Coords", NULL, node );

  if( ! xmlNewProp( coords, (xmlChar*)"points", (xmlChar*)pointsToString(points).c_str() ) ) {
    throw_runtime_error( "PageXML.setCoords: problems setting points attribute" );
    return NULL;
  }

  if( _conf != NULL ) {
    char conf[64];
    snprintf( conf, sizeof conf, "%g", *_conf );
    if( ! xmlNewProp( coords, (xmlChar*)"conf", (xmlChar*)conf ) ) {
      throw_runtime_error( "PageXML.setCoords: problems setting conf attribute" );
      return NULL;
    }
  }

  return coords;
}

/**
 * Adds or modifies (if already exists) the Coords for a given node.
 *
 * @param node   The node of element to set the Coords.
 * @param points Vector of x,y coordinates for the points attribute.
 * @param _conf  Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::setCoords( xmlNodePt node, const vector<cv::Point>& points, const double* _conf ) {
  std::vector<cv::Point2f> points2f;
  cv::Mat(points).convertTo(points2f, cv::Mat(points2f).type());
  return setCoords( node, points2f, _conf );
}

/**
 * Adds or modifies (if already exists) the Coords for a given xpath.
 *
 * @param node   Selector for element to set the Coords.
 * @param points Vector of x,y coordinates for the points attribute.
 * @param _conf  Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::setCoords( const char* xpath, const vector<cv::Point2f>& points, const double* _conf ) {
  vector<xmlNodePt> target = select( xpath );
  if( target.size() == 0 ) {
    throw_runtime_error( "PageXML.setCoords: unmatched target: xpath=%s", xpath );
    return NULL;
  }

  return setCoords( target[0], points, _conf );
}

/**
 * Adds or modifies (if already exists) the Coords as a bounding box for a given node.
 *
 * @param node   The node of element to set the Coords.
 * @param xmin   Minimum x value of bounding box.
 * @param ymin   Minimum y value of bounding box.
 * @param width  Width of bounding box.
 * @param height Height of bounding box.
 * @param _conf  Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::setCoordsBBox( xmlNodePt node, double xmin, double ymin, double width, double height, const double* _conf ) {
  double xmax = xmin+width;
  double ymax = ymin+height;
  vector<cv::Point2f> bbox;
  bbox.push_back( cv::Point2f(xmin,ymin) );
  bbox.push_back( cv::Point2f(xmax,ymin) );
  bbox.push_back( cv::Point2f(xmax,ymax) );
  bbox.push_back( cv::Point2f(xmin,ymax) );

  return setCoords( node, bbox, _conf );
}

/**
 * Adds or modifies (if already exists) the Baseline for a given node.
 *
 * @param node   The node of element to set the Baseline.
 * @param points Vector of x,y coordinates for the points attribute.
 * @param _conf  Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::setBaseline( xmlNodePt node, const vector<cv::Point2f>& points, const double* _conf ) {
  if( ! nodeIs( node, "TextLine" ) ) {
    throw_runtime_error( "PageXML.setBaseline: node is required to be a TextLine" );
    return NULL;
  }

  rmElems( select( "_:Baseline", node ) );

  xmlNodePt baseline;
  vector<xmlNodePt> sel = select( "*[local-name()!='Property' and local-name()!='Coords']", node );
  if( sel.size() > 0 )
    baseline = addElem( "Baseline", NULL, sel[0], PAGEXML_INSERT_PREVSIB );
  else
    baseline = addElem( "Baseline", NULL, node );

  if( ! xmlNewProp( baseline, (xmlChar*)"points", (xmlChar*)pointsToString(points).c_str() ) ) {
    throw_runtime_error( "PageXML.setBaseline: problems setting points attribute" );
    return NULL;
  }

  if( _conf != NULL ) {
    char conf[64];
    snprintf( conf, sizeof conf, "%g", *_conf );
    if( ! xmlNewProp( baseline, (xmlChar*)"conf", (xmlChar*)conf ) ) {
      throw_runtime_error( "PageXML.setBaseline: problems setting conf attribute" );
      return NULL;
    }
  }

  return baseline;
}

/**
 * Adds or modifies (if already exists) the Baseline for a given xpath.
 *
 * @param xpath  Selector for element to set the Baseline.
 * @param points Vector of x,y coordinates for the points attribute.
 * @param _conf  Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::setBaseline( const char* xpath, const vector<cv::Point2f>& points, const double* _conf ) {
  vector<xmlNodePt> target = select( xpath );
  if( target.size() == 0 ) {
    throw_runtime_error( "PageXML.setBaseline: unmatched target: xpath=%s", xpath );
    return NULL;
  }

  return setBaseline( target[0], points, _conf );
}

/**
 * Adds or modifies (if already exists) a two point Baseline for a given node.
 *
 * @param node   The node of element to set the Baseline.
 * @param x1     x value of first point.
 * @param y1     y value of first point.
 * @param x2     x value of second point.
 * @param y2     y value of second point.
 * @param _conf  Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::setBaseline( xmlNodePt node, double x1, double y1, double x2, double y2, const double* _conf ) {
  vector<cv::Point2f> pts;
  pts.push_back( cv::Point2f(x1,y1) );
  pts.push_back( cv::Point2f(x2,y2) );

  return setBaseline( node, pts, _conf );
}

/**
 * Finds the intersection point between two lines defined by pairs of points or returns false if no intersection
 */
bool PageXML::intersection( cv::Point2f line1_point1, cv::Point2f line1_point2, cv::Point2f line2_point1, cv::Point2f line2_point2, cv::Point2f& _ipoint ) {
  cv::Point2f x = line2_point1-line1_point1;
  cv::Point2f direct1 = line1_point2-line1_point1;
  cv::Point2f direct2 = line2_point2-line2_point1;

  double cross = direct1.x*direct2.y - direct1.y*direct2.x;
  if( fabs(cross) < /*EPS*/1e-8 )
    return false;

  double t1 = (x.x * direct2.y - x.y * direct2.x)/cross;
  _ipoint = line1_point1+t1*direct1;

  return true;
}

/**
 * Checks if a point is within a line segment
 *
 * @param segm_start  Point for begining of segment.
 * @param segm_end    Point for end of segment.
 * @param point       Point to check withiness.
 * @return            0 if within segment, +1 if outside but aligned to the right, -1 if outside but aligned to the left, otherwise NaN.
 */
double withinSegment( cv::Point2f segm_start, cv::Point2f segm_end, cv::Point2f point ) {
  cv::Point2f a = segm_start;
  cv::Point2f b = segm_end;
  cv::Point2f c = point;
  double ab = cv::norm(a-b);
  double ac = cv::norm(a-c);
  double bc = cv::norm(b-c);
  double area = fabs( a.x*(b.y-c.y) + b.x*(c.y-a.y) + c.x*(a.y-b.y) ) / (2*(ab+ac+bc)*(ab+ac+bc));

  /// check collinearity (normalized triangle area) ///
  if ( area > 1e-3 )
    return std::numeric_limits<double>::quiet_NaN();
  /// return zero if in segment ///
  if ( ac <= ab && bc <= ab )
    return 0.0;
  /// return +1 if to the right and -1 if to the left ///
  return ac > bc ? 1.0 : -1.0;
}

/**
 * Checks whether Coords is a poly-stripe for its corresponding baseline.
 *
 * @param coords    Coords points.
 * @param baseline  Baseline points.
 * @param offset    The offset of the poly-stripe (>=0 && <= 0.5).
 * @return          Pointer to created element.
 */
bool PageXML::isPolystripe( std::vector<cv::Point2f> coords, std::vector<cv::Point2f> baseline, double* height, double* offset ) {
  if ( baseline.size() == 0 ||
       baseline.size()*2 != coords.size() )
    return false;

  double eps = 1e-2;
  cv::Point2f prevbase;
  cv::Point2f prevabove;
  cv::Point2f prevbelow;

  for ( int n=0; n<(int)baseline.size(); n++ ) {
    int m = coords.size()-1-n;

    /// Check points are collinear ///
    if ( withinSegment( coords[n], coords[m], baseline[n] ) == 0.0 )
      return false;

    /// Check lines are parallel ///
    if ( n > 0 ) {
      prevbase = baseline[n-1]-baseline[n]; prevbase *= 1.0/cv::norm(prevbase);
      prevabove = coords[n-1]-coords[n];    prevabove *= 1.0/cv::norm(prevabove);
      prevbelow = coords[m+1]-coords[m];    prevbelow *= 1.0/cv::norm(prevbelow);
      if ( fabs(1-fabs(prevabove.x*prevbase.x+prevabove.y*prevbase.y)) > eps ||
           fabs(1-fabs(prevbelow.x*prevbase.x+prevbelow.y*prevbase.y)) > eps )
        return false;
    }

    /// Check stripe extremes perpendicular to baseline ///
    if ( n == 0 || n == (int)(baseline.size()-1) ) {
      cv::Point2f base = n > 0 ? prevbase : baseline[1]-baseline[0]; base *= 1.0/cv::norm(base);
      cv::Point2f extr = coords[n]-coords[m]; extr *= 1.0/cv::norm(extr);
      if ( base.x*extr.x+base.y*extr.y > eps )
        return false;
    }
  }

  if ( height != NULL || offset != NULL ) {
    double offup = cv::norm(baseline[0]-coords[0]);
    double offdown = cv::norm(baseline[0]-coords[coords.size()-1]);
    if ( height != NULL )
      *height = offup+offdown;
    if ( offset != NULL )
      *offset = offdown/(offup+offdown);
  }

  return true;
}

/**
 * Sets the Coords of a TextLine as a poly-stripe of the baseline.
 *
 * @param node   The node of element to set the Coords.
 * @param height The height of the poly-stripe in pixels (>0).
 * @param offset The offset of the poly-stripe (>=0 && <= 0.5).
 * @return       Pointer to created element.
 */
xmlNodePt PageXML::setPolystripe( xmlNodePt node, double height, double offset, bool offset_check ) {
  if( ! nodeIs( node, "TextLine" ) ) {
    throw_runtime_error( "PageXML.setPolystripe: node is required to be a TextLine" );
    return NULL;
  }
  if( count( "_:Baseline", node ) == 0 ) {
    throw_runtime_error( "PageXML.setPolystripe: node is required to have a Baseline" );
    return NULL;
  }
  if ( height <= 0 ) {
    throw_runtime_error( "PageXML.setPolystripe: unexpected height" );
    return NULL;
  }
  if ( offset_check && ( offset < 0 || offset > 0.5 ) ) {
    throw_runtime_error( "PageXML.setPolystripe: unexpected offset" );
    return NULL;
  }

  double offup = height - offset*height;
  double offdown = height - offup;

  vector<cv::Point2f> baseline = getPoints( node, "_:Baseline" );
  vector<cv::Point2f> coords;

  cv::Point2f l1p1, l1p2, l2p1, l2p2, base, perp, point;

  for ( int n=0; n<(int)baseline.size()-1; n++ ) {
    base = baseline[n+1]-baseline[n];
    perp = cv::Point2f(base.y,-base.x)*(offup/cv::norm(base));
    l2p1 = baseline[n]+perp;
    l2p2 = baseline[n+1]+perp;
    if ( n == 0 || ! intersection( l1p1, l1p2, l2p1, l2p2, point ) )
      coords.push_back(cv::Point2f(l2p1));
    else
      coords.push_back(cv::Point2f(point));
    l1p1 = l2p1;
    l1p2 = l2p2;
  }
  coords.push_back(cv::Point2f(l2p2));

  for ( int n = baseline.size()-1; n>0; n-- ) {
    base = baseline[n-1]-baseline[n];
    perp = cv::Point2f(base.y,-base.x)*(offdown/cv::norm(base));
    l2p1 = baseline[n]+perp;
    l2p2 = baseline[n-1]+perp;
    if ( n == (int)baseline.size()-1 || ! intersection( l1p1, l1p2, l2p1, l2p2, point ) )
      coords.push_back(cv::Point2f(l2p1));
    else
      coords.push_back(cv::Point2f(point));
    l1p1 = l2p1;
    l1p2 = l2p2;
  }
  coords.push_back(cv::Point2f(l2p2));

  return setCoords( node, coords );
}

/**
 * Gets the page number for the given node.
 */
int PageXML::getPageNumber( xmlNodePt node ) {
  node = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
  vector<xmlNodePt> pages = select("//_:Page");
  for( int n=0; n<(int)pages.size(); n++ )
    if( node == pages[n] )
      return n;
  return -1;
}

/**
 * Sets the image orientation for the given Page node.
 *
 * @param node   The page node.
 * @param angle  The orientation angle in degrees {0,90,180,-90}.
 * @param _conf  Pointer to confidence value, NULL for no confidence.
 */
void PageXML::setPageImageOrientation( xmlNodePt node, int angle, const double* _conf ) {
  if( ! nodeIs( node, "Page" ) ) {
    throw_runtime_error( "PageXML.setPageImageOrientation: node is required to be a Page" );
    return;
  }
  if( angle != 0 && angle != 90 && angle != 180 && angle != -90 ) {
    throw_runtime_error( "PageXML.setPageImageOrientation: the only accepted angle values are: 0, 90, 180 or -90" );
    return;
  }

  rmElems( "_:ImageOrientation", node );

  if( _conf == NULL && angle == 0 )
    return;

  xmlNodePt orientation = addElem( "ImageOrientation", NULL, node, PAGEXML_INSERT_PREPEND );

  setAttr( orientation, "angle", to_string(angle).c_str() );

  if( _conf != NULL ) {
    char conf[32];
    snprintf( conf, sizeof conf, "%g", *_conf );
    setAttr( orientation, "conf", conf );
  }
}
void PageXML::setPageImageOrientation( int pagenum, int angle, const double* _conf ) {
  return setPageImageOrientation( selectNth("//_:Page",pagenum), angle, _conf );
}

/**
 * Gets the image orientation for the given node.
 *
 * @param node   A node to get its image orientation.
 * @return       Orientation in degrees.
 */
int PageXML::getPageImageOrientation( xmlNodePt node ) {
  node = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
  if( ! node ) {
    throw_runtime_error( "PageXML.getPageImageOrientation: node must be a Page or descendant of a Page" );
    return 0;
  }

  node = selectNth( "_:ImageOrientation", 0, node );
  if( ! node )
    return 0;

  string angle = getAttr( node, "angle" );
  return atoi(angle.c_str());
}
int PageXML::getPageImageOrientation( int pagenum ) {
  return getPageImageOrientation( selectNth("//_:Page",pagenum) );
}

/**
 * Returns the width of a page.
 */
unsigned int PageXML::getPageWidth( xmlNodePt node ) {
  node = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
  if( ! nodeIs( node, "Page" ) ) {
    throw_runtime_error( "PageXML.getPageWidth: node is required to be a Page or descendant of a Page" );
    return 0;
  }
  string width = getAttr( node, "imageWidth" );
  return atoi(width.c_str());
}
unsigned int PageXML::getPageWidth( int pagenum ) {
  return getPageWidth( selectNth("//_:Page",pagenum) );
}

/**
 * Returns the height of a page.
 */
unsigned int PageXML::getPageHeight( xmlNodePt node ) {
  node = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
  if( ! nodeIs( node, "Page" ) ) {
    throw_runtime_error( "PageXML.getPageHeight: node is required to be a Page or descendant of a Page" );
    return 0;
  }
  string height = getAttr( node, "imageHeight" );
  return atoi(height.c_str());
}
unsigned int PageXML::getPageHeight( int pagenum ) {
  return getPageHeight( selectNth("//_:Page",pagenum) );
}

/**
 * Retrieves pages size.
 *
 * @param pages      Page nodes.
 * @return           Vector of page sizes.
 */
std::vector<cv::Size2i> PageXML::getPagesSize( std::vector<xmlNodePt> pages ) {
  std::vector<cv::Size2i> sizes;
  for ( int n=0; n<(int)pages.size(); n++ ) {
    if( ! nodeIs( pages[n], "Page" ) ) {
      throw_runtime_error( "PageXML.getPagesSize: node is required to be a Page" );
      return sizes;
    }
    cv::Size2i size( getPageWidth(pages[n]), getPageHeight(pages[n]) );
    sizes.push_back(size);
  }
  return sizes;
}
std::vector<cv::Size2i> PageXML::getPagesSize( const char* xpath ) {
  return getPagesSize( select(xpath) );
}

/**
 * Resizes pages and all respective coordinates.
 *
 * @param sizes               Page sizes to resize to.
 * @param pages               Page nodes.
 * @param check_aspect_ratio  Whether to check that the aspect ratio is properly preserved.
 * @return                    Number of pages+points attributes modified.
 */
int PageXML::resize( std::vector<cv::Size2i> sizes, std::vector<xmlNodePt> pages, bool check_aspect_ratio ) {
  /// Input checks ///
  if ( sizes.size() != pages.size() ) {
    throw_runtime_error( "PageXML.resize: number of sizes and pages must coincide" );
    return 0;
  }
  for ( int n=0; n<(int)pages.size(); n++ )
    if ( ! nodeIs( pages[n], "Page" ) ) {
      throw_runtime_error( "PageXML.resize: all nodes are required to be Page" );
      return 0;
    }

  /// Check that aspect ratios are the same ///
  std::vector<cv::Size2i> orig_sizes = getPagesSize(pages);
  if ( check_aspect_ratio )
    for ( int n=0; n<(int)pages.size(); n++ ) {
      double ratio_diff = sizes[n].width < sizes[n].height ?
        (double)sizes[n].width/sizes[n].height - (double)orig_sizes[n].width/orig_sizes[n].height:
        (double)sizes[n].height/sizes[n].width - (double)orig_sizes[n].height/orig_sizes[n].width;
      if ( fabs(ratio_diff) > 1e-2 ) {
        throw_runtime_error( "PageXML.resize: aspect ratio too different for page %d (%ux%u vs. %ux%u)", n, orig_sizes[n].width, orig_sizes[n].height, sizes[n].width, sizes[n].height );
        return 0;
      }
    }

  /// For each page update size and resize coords ///
  int updated = 0;
  for ( int n=0; n<(int)pages.size(); n++ ) {
    setAttr( pages[n], "imageWidth", std::to_string(sizes[n].width).c_str() );
    setAttr( pages[n], "imageHeight", std::to_string(sizes[n].height).c_str() );
    double fact_x = (double)sizes[n].width/orig_sizes[n].width;
    double fact_y = (double)sizes[n].height/orig_sizes[n].height;

    /// Resize Coords/@points and Baseline/@points ///
    std::vector<xmlNodePt> coords = select( ".//*[@points]", pages[n] );
    for ( int m=0; m<(int)coords.size(); m++ ) {
      std::vector<cv::Point2f> pts = stringToPoints( getAttr(coords[m],"points") );
      for ( int k=(int)pts.size()-1; k>=0; k-- ) {
        pts[k].x *= fact_x;
        pts[k].y *= fact_y;
      }
      setAttr( coords[m], "points", pointsToString(pts).c_str() );
    }

    /// Resize Property[@key='fpgram']/@value ///
    std::vector<xmlNodePt> fpgram = select( ".//_:Property[@key='fpgram' and @value]", pages[n] );
    for ( int m=0; m<(int)fpgram.size(); m++ ) {
      std::vector<cv::Point2f> pts = stringToPoints( getAttr(fpgram[m],"value") );
      for ( int k=(int)pts.size()-1; k>=0; k-- ) {
        pts[k].x *= fact_x;
        pts[k].y *= fact_y;
      }
      setAttr( fpgram[m], "value", pointsToString(pts).c_str() );
    }

    updated += coords.size()+fpgram.size();
  }

  return updated+pages.size();
}

/**
 * Resizes pages and all respective coordinates.
 *
 * @param sizes               Page sizes to resize to.
 * @param xpath               Selector for Page nodes.
 * @param check_aspect_ratio  Whether to check that the aspect ratio is properly preserved.
 * @return                    Number of pages+points attributes modified.
 */
int PageXML::resize( std::vector<cv::Size2i> sizes, const char* xpath, bool check_aspect_ratio ) {
  return resize( sizes, select(xpath), check_aspect_ratio );
}

/**
 * Resizes a page and all respective coordinates.
 *
 * @param size                Page size to resize to.
 * @param page                Page node.
 * @param check_aspect_ratio  Whether to check that the aspect ratio is properly preserved.
 * @return                    Number of pages+points attributes modified.
 */
int PageXML::resize( cv::Size2i size, xmlNodePt page, bool check_aspect_ratio ) {
  std::vector<cv::Size2i> sizes = {size};
  std::vector<xmlNodePt> pages = {page};
  return resize( sizes, pages, check_aspect_ratio );
}

/**
 * Resizes a page and all respective coordinates.
 *
 * @param factor              Resizing factor.
 * @param xpath               Selector for Page nodes.
 * @return                    Number of pages+points attributes modified.
 */
int PageXML::resize( double fact, const char* xpath ) {
  std::vector<xmlNodePt> pages = select(xpath);
  std::vector<cv::Size2i> sizes = getPagesSize(pages);
  for ( int p=0; p<(int)sizes.size(); p++ ) {
    sizes[p].width = std::round(fact*sizes[p].width);
    sizes[p].height = std::round(fact*sizes[p].height);
  }
  return resize( sizes, pages, true );
}

/**
 * Sets the imageFilename of a page.
 */
void PageXML::setPageImageFilename( xmlNodePt node, const char* image ) {
  if( ! nodeIs( node, "Page" ) ) {
    throw_runtime_error( "PageXML.setPageImageFilename: node is required to be a Page" );
    return;
  }
  setAttr( node, "imageFilename", image );
}
void PageXML::setPageImageFilename( int pagenum, const char* image ) {
  return setPageImageFilename( selectNth("//_:Page",pagenum), image );
}

/**
 * Returns the imageFilename of a page.
 */
string PageXML::getPageImageFilename( xmlNodePt node ) {
  node = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
  if( ! nodeIs( node, "Page" ) ) {
    throw_runtime_error( "PageXML.getPageImageFilename: node is required to be a Page or descendant of a Page" );
    return string("");
  }
  return getAttr( node, "imageFilename" );
}
string PageXML::getPageImageFilename( int pagenum ) {
  return getPageImageFilename( selectNth("//_:Page",pagenum) );
}

/**
 * Returns the image for the given page.
 */
PageImage PageXML::getPageImage( int pagenum ) {
  if( pagenum < 0 || pagenum >= (int)pagesImage.size() ) {
    throw_runtime_error( "PageXML.getPageImage: page number out of range" );
    PageImage pageImage;
    return pageImage;
  }

#if defined (__PAGEXML_LEPT__)
  if( pagesImage[pagenum] == NULL )
#elif defined (__PAGEXML_MAGICK__)
  if( pagesImage[pagenum].columns() == 0 )
#elif defined (__PAGEXML_CVIMG__)
  if( ! pagesImage[pagenum].data )
#endif
    loadImage(pagenum);

  return pagesImage[pagenum];
}
PageImage PageXML::getPageImage( xmlNodePt node ) {
  return getPageImage( getPageNumber(node) );
}

/**
 * Adds a Glyph to a given node.
 *
 * @param node       The node of element to add the Glyph.
 * @param id         ID for Glyph, if NULL it is selected automatically.
 * @param before_id  If !=NULL inserts it before the Glyph with this ID.
 * @return           Pointer to created element.
 */
xmlNodePt PageXML::addGlyph( xmlNodePt node, const char* id, const char* before_id ) {
  if( ! nodeIs( node, "Word" ) ) {
    throw_runtime_error( "PageXML.addGlyph: node is required to be a Word" );
    return NULL;
  }

  xmlNodePt glyph;

  string gid;
  if( id != NULL )
    gid = string(id);
  else {
    string wid = getAttr( node, "id" );
    if( wid.empty() ) {
      throw_runtime_error( "PageXML.addGlyph: expected element to have an id attribute" );
      return NULL;
    }
    int n = select( "_:Glyph", node ).size();
    while( true ) {
      if( select( string("*[@id='")+wid+"_g"+to_string(++n)+"']", node ).size() == 0 ) {
        gid = wid+"_g"+to_string(n);
        break;
      }
      if( n > 100000 ) {
        throw_runtime_error( "PageXML.addGlyph: apparently in infinite loop" );
        return NULL;
      }
    }
  }

  if( before_id != NULL ) {
    vector<xmlNodePt> sel = select( string("*[@id='")+before_id+"']", node );
    if( sel.size() == 0 ) {
      throw_runtime_error( "PageXML.addGlyph: unable to find id=%s", before_id );
      return NULL;
    }
    glyph = addElem( "Glyph", gid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
  }
  else {
    vector<xmlNodePt> sel = select( "_:TextEquiv", node );
    if( sel.size() > 0 )
      glyph = addElem( "Glyph", gid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
    else
      glyph = addElem( "Glyph", gid.c_str(), node, PAGEXML_INSERT_APPEND, true );
  }

  return glyph;
}

/**
 * Adds a Glyph to a given xpath.
 *
 * @param xpath      Selector for element to set the Glyph.
 * @param id         ID for Glyph, if NULL it is selected automatically.
 * @param before_id  If !=NULL inserts it before the Glyph with this ID.
 * @return           Pointer to created element.
 */
xmlNodePt PageXML::addGlyph( const char* xpath, const char* id, const char* before_id ) {
  vector<xmlNodePt> target = select( xpath );
  if( target.size() == 0 ) {
    throw_runtime_error( "PageXML.addGlyph: unmatched target: xpath=%s", xpath );
    return NULL;
  }

  return addGlyph( target[0], id, before_id );
}

/**
 * Adds a Word to a given node.
 *
 * @param node       The node of element to add the Word.
 * @param id         ID for Word, if NULL it is selected automatically.
 * @param before_id  If !=NULL inserts it before the Word with this ID.
 * @return           Pointer to created element.
 */
xmlNodePt PageXML::addWord( xmlNodePt node, const char* id, const char* before_id ) {
  if( ! nodeIs( node, "TextLine" ) ) {
    throw_runtime_error( "PageXML.addWord: node is required to be a TextLine" );
    return NULL;
  }

  xmlNodePt word;

  string wid;
  if( id != NULL )
    wid = string(id);
  else {
    string lid = getAttr( node, "id" );
    if( lid.empty() ) {
      throw_runtime_error( "PageXML.addWord: expected element to have an id attribute" );
      return NULL;
    }
    int n = select( "_:Word", node ).size();
    while( true ) {
      if( select( string("*[@id='")+lid+"_w"+to_string(++n)+"']", node ).size() == 0 ) {
        wid = lid+"_w"+to_string(n);
        break;
      }
      if( n > 100000 ) {
        throw_runtime_error( "PageXML.addWord: apparently in infinite loop" );
        return NULL;
      }
    }
  }

  if( before_id != NULL ) {
    vector<xmlNodePt> sel = select( string("*[@id='")+before_id+"']", node );
    if( sel.size() == 0 ) {
      throw_runtime_error( "PageXML.addWord: unable to find id=%s", before_id );
      return NULL;
    }
    word = addElem( "Word", wid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
  }
  else {
    vector<xmlNodePt> sel = select( "_:TextEquiv", node );
    if( sel.size() > 0 )
      word = addElem( "Word", wid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
    else
      word = addElem( "Word", wid.c_str(), node, PAGEXML_INSERT_APPEND, true );
  }

  return word;
}

/**
 * Adds a Word to a given xpath.
 *
 * @param xpath      Selector for element to set the Word.
 * @param id         ID for Word, if NULL it is selected automatically.
 * @param before_id  If !=NULL inserts it before the Word with this ID.
 * @return           Pointer to created element.
 */
xmlNodePt PageXML::addWord( const char* xpath, const char* id, const char* before_id ) {
  vector<xmlNodePt> target = select( xpath );
  if( target.size() == 0 ) {
    throw_runtime_error( "PageXML.addWord: unmatched target: xpath=%s", xpath );
    return NULL;
  }

  return addWord( target[0], id, before_id );
}

/**
 * Adds a TextLine to a given node.
 *
 * @param node       The node of element to add the TextLine.
 * @param id         ID for TextLine, if NULL it is selected automatically.
 * @param before_id  If !=NULL inserts it before the TextLine with this ID.
 * @return           Pointer to created element.
 */
xmlNodePt PageXML::addTextLine( xmlNodePt node, const char* id, const char* before_id ) {
  if( ! nodeIs( node, "TextRegion" ) ) {
    throw_runtime_error( "PageXML.addTextLine: node is required to be a TextRegion" );
    return NULL;
  }

  xmlNodePt textline;

  string lid;
  if( id != NULL )
    lid = string(id);
  else {
    string rid = getAttr( node, "id" );
    if( rid.empty() ) {
      throw_runtime_error( "PageXML.addTextLine: expected element to have an id attribute" );
      return NULL;
    }
    int n = select( "_:TextLine", node ).size();
    while( true ) {
      if( select( string("*[@id='")+rid+"_l"+to_string(++n)+"']", node ).size() == 0 ) {
        lid = rid+"_l"+to_string(n);
        break;
      }
      if( n > 100000 ) {
        throw_runtime_error( "PageXML.addTextLine: apparently in infinite loop" );
        return NULL;
      }
    }
  }

  if( before_id != NULL ) {
    vector<xmlNodePt> sel = select( string("*[@id='")+before_id+"']", node );
    if( sel.size() == 0 ) {
      throw_runtime_error( "PageXML.addTextLine: unable to find id=%s", before_id );
      return NULL;
    }
    textline = addElem( "TextLine", lid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
  }
  else {
    vector<xmlNodePt> sel = select( "_:TextEquiv", node );
    if( sel.size() > 0 )
      textline = addElem( "TextLine", lid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
    else
      textline = addElem( "TextLine", lid.c_str(), node, PAGEXML_INSERT_APPEND, true );
  }

  return textline;
}

/**
 * Adds a TextLine to a given xpath.
 *
 * @param xpath      Selector for element to add the TextLine.
 * @param id         ID for TextLine, if NULL it is selected automatically.
 * @param before_id  If !=NULL inserts it before the TextLine with this ID.
 * @return           Pointer to created element.
 */
xmlNodePt PageXML::addTextLine( const char* xpath, const char* id, const char* before_id ) {
  vector<xmlNodePt> target = select( xpath );
  if( target.size() == 0 ) {
    throw_runtime_error( "PageXML.addTextLine: unmatched target: xpath=%s", xpath );
    return NULL;
  }

  return addTextLine( target[0], id, before_id );
}

/**
 * Adds a TextRegion to a given node.
 *
 * @param node       The node of element to add the TextRegion.
 * @param id         ID for TextRegion, if NULL it is selected automatically.
 * @param before_id  If !=NULL inserts it before the TextRegion with this ID.
 * @return           Pointer to created element.
 */
xmlNodePt PageXML::addTextRegion( xmlNodePt node, const char* id, const char* before_id ) {
  if( ! nodeIs( node, "Page" ) ) {
    throw_runtime_error( "PageXML.addTextRegion: node is required to be a Page" );
    return NULL;
  }

  xmlNodePt textreg;

  string rid;
  if( id != NULL )
    rid = string(id);
  else {
    int n = select( "*/_:TextRegion", node->parent ).size();
    while( true ) {
      if( select( string("*/*[@id='t")+to_string(++n)+"']", node->parent ).size() == 0 ) {
        rid = string("t")+to_string(n);
        break;
      }
      if( n > 100000 ) {
        throw_runtime_error( "PageXML.addTextRegion: apparently in infinite loop" );
        return NULL;
      }
    }
  }

  if( before_id != NULL ) {
    vector<xmlNodePt> sel = select( string("*[@id='")+before_id+"']", node );
    if( sel.size() == 0 ) {
      throw_runtime_error( "PageXML.addTextRegion: unable to find id=%s", before_id );
      return NULL;
    }
    textreg = addElem( "TextRegion", rid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
  }
  else {
    vector<xmlNodePt> sel = select( "_:TextEquiv", node );
    if( sel.size() > 0 )
      textreg = addElem( "TextRegion", rid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
    else
      textreg = addElem( "TextRegion", rid.c_str(), node, PAGEXML_INSERT_APPEND, true );
  }

  return textreg;
}

/**
 * Adds new TextRegion to a given xpath.
 *
 * @param xpath      Selector for element to add the TextRegion.
 * @param id         ID for TextRegion, if NULL it is selected automatically.
 * @param before_id  If !=NULL inserts it before the TextRegion with this ID.
 * @return           Pointer to created element.
 */
xmlNodePt PageXML::addTextRegion( const char* xpath, const char* id, const char* before_id ) {
  if( xpath == NULL )
    xpath = "//_:Page";
  vector<xmlNodePt> target = select( xpath );
  if( target.size() == 0 ) {
    throw_runtime_error( "PageXML.addTextRegion: unmatched target: xpath=%s", xpath );
    return NULL;
  }

  return addTextRegion( target[0], id, before_id );
}

/**
 * Adds a Page to the PcGts node.
 *
 * @param image        Path to the image file.
 * @param imgW         Width of image.
 * @param imgH         Height of image.
 * @param id           ID for Page, if NULL it is left unset.
 * @param before_node  If !=NULL inserts it before the provided Page node.
 * @return             Pointer to created element.
 */
xmlNodePt PageXML::addPage( const char* image, const int imgW, const int imgH, const char* id, xmlNodePt before_node ) {
  xmlNodePt page;

  PageImage pageImage;
  string imageFilename;
  string imageBase;

  int page_num = -1;

  if( before_node != NULL ) {
    if( ! nodeIs( before_node, "Page" ) ) {
      throw_runtime_error( "PageXML.addPage: before_node is required to be a Page" );
      return NULL;
    }
    page = addElem( "Page", id, before_node, PAGEXML_INSERT_PREVSIB, true );
    page_num = getPageNumber(page);

    int numpages = pagesImage.size();
    pagesImage.push_back(pagesImage[numpages-1]);
    pagesImageFilename.push_back(pagesImageFilename[numpages-1]);
    pagesImageBase.push_back(pagesImageBase[numpages-1]);
    for( int n=numpages-1; n>page_num; n-- ) {
      pagesImage[n] = pagesImage[n-1];
      pagesImageFilename[n] = pagesImageFilename[n-1];
      pagesImageBase[n] = pagesImageBase[n-1];
    }
  }
  else {
    xmlNodePt pcgts = selectNth("/_:PcGts",0);
    if( ! pcgts ) {
      throw_runtime_error( "PageXML.addPage: unable to select PcGts node" );
      return NULL;
    }
    page = addElem( "Page", id, pcgts, PAGEXML_INSERT_APPEND, true );
    page_num = getPageNumber(page);

    pagesImage.push_back(pageImage);
    pagesImageFilename.push_back(imageFilename);
    pagesImageBase.push_back(imageBase);
  }

  setAttr( page, "imageFilename", image );
  // @todo If size is zero, read the image and get size like in newXml
  setAttr( page, "imageWidth", to_string(imgW).c_str() );
  setAttr( page, "imageHeight", to_string(imgH).c_str() );

  parsePageImage( page_num );

  return page;
}

/**
 * Adds a Page to the PcGts node.
 *
 * @param image        Path to the image file.
 * @param imgW         Width of image.
 * @param imgH         Height of image.
 * @param id           ID for Page, if NULL it is left unset.
 * @param before_node  If !=NULL inserts it before the provided Page node.
 * @return             Pointer to created element.
 */
xmlNodePt PageXML::addPage( std::string image, const int imgW, const int imgH, const char* id, xmlNodePt before_node ) {
  return addPage(image.c_str(),imgW,imgH,id,before_node);
}

/**
 * Gets image bases for all pages in xml.
 *
 * @return  Vector of strings containing the image base names.
 */
std::vector<std::string> PageXML::getImageBases() {
  return pagesImageBase;
}

/**
 * Verifies that all IDs in page are unique.
 */
bool PageXML::areIDsUnique() {
  string id;
  bool unique = true;
  map<string,bool> seen;

  vector<xmlNodePt> nodes = select( "//*[@id]" );
  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
    id = getAttr( nodes[n], "id" );
    if( seen.find(id) != seen.end() && seen[id] ) {
      fprintf( stderr, "PageXML.areIDsUnique: duplicate ID: %s\n", id.c_str() );
      seen[id] = unique = false;
    }
    else
      seen[id] = true;
  }

  return unique;
}

/**
 * Simplifies IDs by removing imgbase prefixes and replaces invalid characters with _.
 *
 * @return       Number of IDs simplified.
 */
int PageXML::simplifyIDs() {
  int simplified = 0;

  regex reTrim("^[^a-zA-Z]*");
  regex reInvalid("[^a-zA-Z0-9_-]");
  string sampbase;
  xmlNodePt prevPage = NULL;

  vector<xmlNodePt> nodes = select( "//*[@id][local-name()='TextLine' or local-name()='TextRegion']" );

  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
    xmlNodePt page = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, nodes[n] );
    if( prevPage != page ) {
      prevPage = page;
      sampbase = pagesImageBase[getPageNumber(page)];
    }
    char* id = (char*)xmlGetProp( nodes[n], (xmlChar*)"id" );
    string sampid(id);
    if( sampid.size() > sampbase.size() &&
        ! sampid.compare(0,sampbase.size(),sampbase) ) {
      sampid.erase( 0, sampbase.size() );
      sampid = regex_replace(sampid,reTrim,"");
      if( sampid.size() > 0 ) {
        sampid = regex_replace(sampid,reInvalid,"_");
        setAttr( nodes[n], "orig-id", id );
        setAttr( nodes[n], "id", sampid.c_str() );
        simplified++;
      }
    }
    free(id);
  }

  return simplified;
}

/**
 * Modifies imageFilename to be a relative path w.r.t. given xml path. Currently just checks prefix directories and removes it.
 */
void PageXML::relativizeImageFilename( const char* xml_path ) {
  string xml_base = regex_replace(xml_path,regex("/[^/]+$"),"/");

  vector<xmlNodePt> pages = select( "//_:Page" );

  for ( int n=(int)pages.size()-1; n>=0; n-- ) {
    std::string img = getAttr( pages[n], "imageFilename" );
    if ( img.compare(0, xml_base.length(), xml_base) == 0 ) {
      img.erase(0,xml_base.length());
      setAttr( pages[n], "imageFilename", img.c_str() );
    }
  }
}


#if defined (__PAGEXML_OGR__)

/**
 * Gets an element's Coords as an OGRMultiPolygon.
 *
 * @param node       The element from which to extract the Coords points.
 * @param xpath      Selector for the Coords element.
 * @return           Pointer to OGRMultiPolygon element.
 */
OGRMultiPolygon* PageXML::getOGRpolygon( const xmlNodePt node, const char* xpath ) {
  std::vector<cv::Point2f> pts = getPoints(node,xpath);

  OGRLinearRing* ring = new OGRLinearRing();
  OGRPolygon* poly = new OGRPolygon();

  for ( int n=0; n<(int)pts.size(); n++ )
    ring->addPoint(pts[n].x, pts[n].y);
  ring->closeRings();
  poly->addRing(ring);

  return (OGRMultiPolygon*)OGRGeometryFactory::forceToMultiPolygon(poly);
}

/**
 * Gets elements' Coords as OGRMultiPolygons.
 *
 * @param nodes      Elements from which to extract the Coords points.
 * @param xpath      Selector for the Coords element.
 * @return           Vector of OGRMultiPolygon pointer elements.
 */
std::vector<OGRMultiPolygon*> PageXML::getOGRpolygons( std::vector<xmlNodePt> nodes, const char* xpath ) {
  std::vector<OGRMultiPolygon*> polys;
  for ( int n=0; n<(int)nodes.size(); n++ )
    polys.push_back( getOGRpolygon(nodes[n],xpath) );
  return polys;
}

/**
 * Gets the union of Coords elements as a OGRMultiPolygon.
 *
 * @param nodes      Elements from which to extract the Coords points.
 * @param xpath      Selector for the Coords element.
 * @return           Pointer to OGRMultiPolygon element.
 */
OGRMultiPolygon* PageXML::getUnionOGRpolygon( std::vector<xmlNodePt> nodes, const char* xpath ) {
  OGRGeometry* geo_union = getOGRpolygon(nodes[0],xpath);
  for ( int n=1; n<(int)nodes.size(); n++ )
    geo_union = geo_union->Union( getOGRpolygon(nodes[n],xpath) );
  return ((OGRMultiPolygon*)OGRGeometryFactory::forceToMultiPolygon(geo_union));
}

/**
 * Gets the area of a OGRMultiPolygon.
 *
 * @param poly       OGRMultiPolygon pointer.
 * @return           Area.
 */
double PageXML::getOGRpolygonArea( OGRMultiPolygon* poly ) {
  return poly->get_Area();
}

/**
 * Gets the element's Baseline as an OGRMultiLineString.
 *
 * @param node       The element from which to extract the Baseline points.
 * @return           Pointer to OGRMultiLineString element.
 */
OGRMultiLineString* PageXML::getOGRpolyline( const xmlNodePt node, const char* xpath ) {
  std::vector<cv::Point2f> pts = getPoints(node,xpath);

  OGRLineString* curve = new OGRLineString();

  for ( int n=0; n<(int)pts.size(); n++ )
    curve->addPoint(pts[n].x, pts[n].y);

  return (OGRMultiLineString*)OGRGeometryFactory::forceToMultiLineString(curve);
}

/**
 * Computes the intersection factor of one polygon over another.
 *
 * @param poly1      First polygon.
 * @param poly2      Second polygon.
 * @return           Factor value.
 */
double PageXML::computeIntersectFactor( OGRMultiPolygon* poly1, OGRMultiPolygon* poly2 ) {
  OGRGeometry *isect_geom = poly1->Intersection(poly2);
  int isect_type = isect_geom->getGeometryType();
  if ( isect_type != wkbPolygon &&
       isect_type != wkbMultiPolygon &&
       isect_type != wkbGeometryCollection )
    return 0.0;

  return ((OGRMultiPolygon*)OGRGeometryFactory::forceToMultiPolygon(isect_geom))->get_Area()/poly1->get_Area();
}

/**
 * Computes the intersection factor of one polyline over polygon.
 *
 * @param poly1      Polyline.
 * @param poly2      Polygon.
 * @return           Factor value.
 */
double PageXML::computeIntersectFactor( OGRMultiLineString* poly1, OGRMultiPolygon* poly2 ) {
  OGRGeometry *isect_geom = poly2->Intersection(poly1);
  int isect_type = isect_geom->getGeometryType();
  if ( isect_type != wkbLineString &&
       isect_type != wkbMultiLineString &&
       isect_type != wkbGeometryCollection )
    return 0.0;

  return ((OGRMultiLineString*)OGRGeometryFactory::forceToMultiLineString(isect_geom))->get_Length()/poly1->get_Length();
}

/**
 * Computes the intersection over union (IoU) of two polygons.
 *
 * @param poly1      First polygon.
 * @param poly2      Second polygon.
 * @return           IoU value.
 */
double PageXML::computeIoU( OGRMultiPolygon* poly1, OGRMultiPolygon* poly2 ) {
  OGRGeometry *isect_geom = poly1->Intersection(poly2);
  int isect_type = isect_geom->getGeometryType();
  if ( isect_type != wkbPolygon &&
       isect_type != wkbMultiPolygon &&
       isect_type != wkbGeometryCollection )
    return 0.0;

  double iou = ((OGRMultiPolygon*)OGRGeometryFactory::forceToMultiPolygon(isect_geom))->get_Area();
  if ( iou != 0.0 )
    iou /= poly1->get_Area()+poly2->get_Area()-iou;

  return iou;
}

/**
 * Computes the intersection over unions (IoU) of polygons.
 *
 * @param poly      Polygon.
 * @param polys     Vector of polygons.
 * @param ious      IoU values.
 */
void PageXML::computeIoUs( OGRMultiPolygon* poly, std::vector<OGRMultiPolygon*> polys, std::vector<double>& ious ) {
  ious.clear();
  for ( int n=0; n<(int)polys.size(); n++ )
    ious.push_back( computeIoU(poly,polys[n]) );
}

/**
 * Computes coords-region intersections weighted by area.
 *
 * @param line       TextLine element.
 * @param regs       TextRegion elements.
 * @param reg_polys  Extracted OGR polygons.
 * @param reg_areas  Region areas.
 * @param scores     Obtained intersection scores.
 */
void PageXML::computeCoordsIntersectionsWeightedByArea( xmlNodePtr line, std::vector<xmlNodePtr> regs, std::vector<OGRMultiPolygon*>& reg_polys, std::vector<double>& reg_areas, std::vector<double>& scores ) {
  /// Get region polygons ///
  if ( regs.size() != reg_polys.size() || reg_polys.size() == 0 ) {
    reg_polys.clear();
    for ( int n=0; n<(int)regs.size(); n++ )
      reg_polys.push_back( getOGRpolygon(regs[n]) );
  }
  /// Compute region areas ///
  if ( regs.size() != reg_areas.size() || reg_areas.size() == 0 ) {
    reg_areas.clear();
    for ( int n=0; n<(int)reg_polys.size(); n++ )
      reg_areas.push_back( reg_polys[n]->get_Area() );
  }

  /// Compute baseline intersections ///
  scores.clear();
  OGRMultiPolygon *coords = getOGRpolygon( line );
  double coords_area = coords->get_Area();
  double sum_areas = 0.0;
  int isect_count = 0;
  for ( int n=0; n<(int)reg_polys.size(); n++ ) {
    OGRGeometry *isect_geom = reg_polys[n]->Intersection(coords);
    double isect_area = ((OGRMultiLineString*)OGRGeometryFactory::forceToMultiPolygon(isect_geom))->get_Area();
    scores.push_back( isect_area <= 0.0 ? 0.0 : isect_area/coords_area );
    if ( isect_area > 0.0 ) {
      sum_areas += reg_areas[n];
      isect_count++;
    }
  }

  /// Return if fewer than 2 intersects ///
  if ( isect_count < 2 )
    return;

  /// Weight by areas ///
  for ( int n=0; n<(int)scores.size(); n++ )
    if ( scores[n] > 0.0 )
      scores[n] *= 1.0-reg_areas[n]/sum_areas;
}

/**
 * Computes baseline-region intersections weighted by area.
 *
 * @param line       TextLine element.
 * @param regs       TextRegion elements.
 * @param reg_polys  Extracted OGR polygons.
 * @param reg_areas  Region areas.
 * @param scores     Obtained intersection scores.
 */
void PageXML::computeBaselineIntersectionsWeightedByArea( xmlNodePtr line, std::vector<xmlNodePtr> regs, std::vector<OGRMultiPolygon*>& reg_polys, std::vector<double>& reg_areas, std::vector<double>& scores ) {
  /// Get region polygons ///
  if ( regs.size() != reg_polys.size() || reg_polys.size() == 0 ) {
    reg_polys.clear();
    for ( int n=0; n<(int)regs.size(); n++ )
      reg_polys.push_back( getOGRpolygon(regs[n]) );
  }
  /// Compute region areas ///
  if ( regs.size() != reg_areas.size() || reg_areas.size() == 0 ) {
    reg_areas.clear();
    for ( int n=0; n<(int)reg_polys.size(); n++ )
      reg_areas.push_back( reg_polys[n]->get_Area() );
  }

  /// Compute baseline intersections ///
  scores.clear();
  OGRMultiLineString *baseline = getOGRpolyline( line );
  double baseline_length = baseline->get_Length();
  double sum_areas = 0.0;
  int isect_count = 0;
  for ( int n=0; n<(int)reg_polys.size(); n++ ) {
    OGRGeometry *isect_geom = reg_polys[n]->Intersection(baseline);
    double isect_lgth = ((OGRMultiLineString*)OGRGeometryFactory::forceToMultiLineString(isect_geom))->get_Length();
    scores.push_back( isect_lgth <= 0.0 ? 0.0 : isect_lgth/baseline_length );
    if ( isect_lgth > 0.0 ) {
      sum_areas += reg_areas[n];
      isect_count++;
    }
  }

  /// Return if fewer than 2 intersects ///
  if ( isect_count < 2 )
    return;

  /// Weight by areas ///
  for ( int n=0; n<(int)scores.size(); n++ )
    if ( scores[n] > 0.0 )
      scores[n] *= 1.0-reg_areas[n]/sum_areas;
}

/**
 * Copies TextLines from one page xml to another assigning to regions based on overlap.
 *
 * @param pageFrom      PageXML from where to copy TextLines.
 * @param overlap_type  Type of overlap to use for assigning lines to regions.
 * @param overlap_fact  Overlapping factor.
 * @return              Number of TextLines copied.
 */
int PageXML::copyTextLinesAssignByOverlap( PageXML& pageFrom, PAGEXML_OVERLAP overlap_type, double overlap_fact ) {
  xmlDocPtr docToPtr = getDocPtr();
  std::vector<xmlNodePtr> pgsFrom = pageFrom.select("//_:Page");
  std::vector<xmlNodePtr> pgsTo = select("//_:Page");

  if ( pgsFrom.size() != pgsTo.size() ) {
    throw_runtime_error( "PageXML.copyTextLinesAssignByOverlap: PageXML objects must have the same number of pages" );
    return 0;
  }

  int linesCopied = 0;

  /// Loop through pages ///
  for ( int npage = 0; npage<(int)pgsFrom.size(); npage++ ) {
    std::vector<xmlNodePtr> linesFrom = pageFrom.select( ".//_:TextLine", pgsFrom[npage] );
    if( linesFrom.size() == 0 )
      continue;

    unsigned int toImW = getPageWidth(npage);
    unsigned int toImH = getPageHeight(npage);
    /// Check that image size is the same in both PageXMLs ///
    if ( toImW != pageFrom.getPageWidth(npage) ||
         toImH != pageFrom.getPageHeight(npage) ) {
      throw_runtime_error( "PageXML.copyTextLinesAssignByOverlap: for Page %d image size differs between input PageXMLs", npage );
      return 0;
    }

    /// Select page region or create one if it does not exist ///
    std::string xmax = std::to_string(toImW-1);
    std::string ymax = std::to_string(toImH-1);
    xmlNodePtr pageRegTo = selectNth( std::string("_:TextRegion[_:Coords[@points='0,0 ")+xmax+",0 "+xmax+","+ymax+" 0,"+ymax+"']]", 0, pgsTo[npage] );
    bool pageregadded = false;
    if ( ! pageRegTo ) {
      pageregadded = true;
      pageRegTo = addTextRegion( pgsTo[npage], (std::string("page")+std::to_string(npage+1)).c_str() );
      setCoordsBBox( pageRegTo, 0, 0, toImW-1, toImH-1 );
    }

    /// Select relevant elements ///
    std::vector<xmlNodePtr> linesTo = select( ".//_:TextLine", pgsTo[npage] );
    std::vector<xmlNodePtr> regsTo = select( ".//_:TextRegion", pgsTo[npage] );

    /// Get polygons of regions for IoU computation ///
    std::vector<OGRMultiPolygon*> regs_poly = getOGRpolygons(regsTo);

    /// Loop through lines ///
    std::vector<xmlNodePtr> linesAdded;
    std::vector<double> reg_areas;
    for ( int n=0; n<(int)linesFrom.size(); n++ ) {
      /// Compute overlap scores ///
      std::vector<double> overlap;
      std::vector<double> overlap2;
      switch ( overlap_type ) {
        case PAGEXML_OVERLAP_COORDS_IOU:
          computeIoUs( pageFrom.getOGRpolygon(linesFrom[n]), regs_poly, overlap );
          break;
        case PAGEXML_OVERLAP_COORDS_IWA:
          computeCoordsIntersectionsWeightedByArea( linesFrom[n], regsTo, regs_poly, reg_areas, overlap );
          break;
        case PAGEXML_OVERLAP_BASELINE_IWA:
          computeBaselineIntersectionsWeightedByArea( linesFrom[n], regsTo, regs_poly, reg_areas, overlap );
          break;
        case PAGEXML_OVERLAP_COORDS_BASELINE_IWA:
          computeBaselineIntersectionsWeightedByArea( linesFrom[n], regsTo, regs_poly, reg_areas, overlap );
          computeCoordsIntersectionsWeightedByArea( linesFrom[n], regsTo, regs_poly, reg_areas, overlap2 );
          for ( int m=0; m<(int)overlap.size(); m++ )
            overlap[m] = overlap_fact*overlap[m]+(1-overlap_fact)*overlap2[m];
          break;
      }

      /// Clone line and add it to the destination region node ///
      xmlNodePtr lineclone = NULL;
      if ( 0 != xmlDOMWrapCloneNode( NULL, NULL, linesFrom[n], &lineclone, docToPtr, NULL, 1, 0 ) ||
          lineclone == NULL ) {
        throw_runtime_error( "PageXML.copyTextLinesAssignByOverlap: problems cloning TextLine node" );
        return 0;
      }
      int max_idx = std::distance(overlap.begin(), std::max_element(overlap.begin(), overlap.end()));
      if ( overlap[max_idx] == 0.0 )
        throw_runtime_error( "PageXML.copyTextLinesAssignByOverlap: TextLine does not overlap with any region" );
      xmlAddChild(regsTo[max_idx],lineclone);
    }

    /// Remove added page region if no TextLine was added to it ///
    if ( pageregadded && count("_:TextLine",pageRegTo) == 0 )
      rmElem(pageRegTo);

    linesCopied += linesFrom.size();
  }

  return linesCopied;
}

#endif

/**
 * Tests for text line continuation (requires single segment polystripe).
 *
 * @param lines                 TextLine elements to test for continuation.
 * @param _line_group_order     Join groups line indices (output).
 * @param _line_group_score     Join group scores (output).
 * @param cfg_max_angle_diff    Maximum baseline angle difference for joining.
 * @param cfg_max_horiz_iou     Maximum horizontal IoU for joining.
 * @param cfg_min_prolong_fact  Minimum prolongation factor for joining.
 * @param fake_baseline         Use bottom line of Coords rectangle as the baseline.
 * @return                      Number of join groups.
 */
int PageXML::testTextLineContinuation( std::vector<xmlNodePt> lines, std::vector<std::vector<int> >& _line_group_order, std::vector<double>& _line_group_score, double cfg_max_angle_diff, double cfg_max_horiz_iou, double cfg_min_prolong_fact, bool fake_baseline ) {
  /// Get points and compute baseline angles and lengths ///
  std::vector< std::vector<cv::Point2f> > coords;
  std::vector< std::vector<cv::Point2f> > baseline;
  std::vector<double> angle;
  std::vector<double> length;
  int num_lines = lines.size();
  for ( int n=0; n<num_lines; n++ ) {
    coords.push_back( getPoints(lines[n]) );
    if ( fake_baseline ) {
      if ( coords[n].size() != 4 ) {
        throw_runtime_error( "PageXML.testTextLineContinuation: fake_baseline requires Coords to have exactly 4 points" );
        return -1;
      }
      std::vector<cv::Point2f> baseline_n;
      baseline_n.push_back( cv::Point2f(coords[n][3]) );
      baseline_n.push_back( cv::Point2f(coords[n][2]) );
      baseline.push_back(baseline_n);
    }
    else
      baseline.push_back( getPoints(lines[n],"_:Baseline") );
    angle.push_back(getBaselineOrientation(baseline[n]));
    length.push_back(getBaselineLength(baseline[n]));

    if ( ! nodeIs( lines[n], "TextLine" ) ) {
      throw_runtime_error( "PageXML.testTextLineContinuation: input nodes need to be TextLines" );
      return -1;
    }
    // @todo Check for single segment polystripe 
    if ( baseline[n].size() != 2 || coords[n].size() != 4 ) {
      throw_runtime_error( "PageXML.testTextLineContinuation: Baselines and Coords are required to have exactly 2 and 4 points respectively" );
      return -1;
    }
  }

  std::vector<std::unordered_set<int> > line_groups;
  std::vector<std::vector<int> > line_group_order;
  std::vector<std::vector<double> > line_group_scores;
  std::vector<double> line_group_direct;

  /// Loop through all directed pairs of text lines ///
  for ( int n=0; n<num_lines; n++ )
    for ( int m=0; m<num_lines; m++ )
      if ( n != m ) {
        /// Check that baseline angle difference is small ///
        double angle_diff = fabs(angleDiff(angle[n],angle[m]));
        if ( angle_diff > cfg_max_angle_diff )
          continue;

        /// Project baseline limits onto the local horizontal axis ///
        cv::Point2f dir_n = baseline[n][1]-baseline[n][0];
        cv::Point2f dir_m = baseline[m][1]-baseline[m][0];
        dir_n *= 1.0/cv::norm(dir_n);
        dir_m *= 1.0/cv::norm(dir_m);
        cv::Point2f horiz = (length[n]*dir_n+length[m]*dir_m)*(1.0/(length[n]+length[m]));

        std::vector<double> horiz_n = project_2d_to_1d(baseline[n],horiz);
        std::vector<double> horiz_m = project_2d_to_1d(baseline[m],horiz);

        /// Check that line n starts before line m ///
        double direct = horiz_n[0] < horiz_n[1] ? 1.0 : -1.0;
        if ( direct*horiz_m[0] < direct*horiz_n[0] )
          continue;
        
        /// Check that horizontal IoU is small //
        double iou = IoU_1d(horiz_n[0],horiz_n[1],horiz_m[0],horiz_m[1]);
        if ( iou > cfg_max_horiz_iou )
          continue;

        /// Compute coords endpoint-startpoint intersection factors ///
        std::vector<cv::Point2f> pts_n = coords[n];
        std::vector<cv::Point2f> pts_m = coords[m];
        std::vector<cv::Point2f> isect_nm(2);
        std::vector<cv::Point2f> isect_mn(2);

        if ( ! intersection( pts_n[0], pts_n[1], pts_m[0], pts_m[3], isect_nm[0] ) ) continue;
        if ( ! intersection( pts_n[3], pts_n[2], pts_m[0], pts_m[3], isect_nm[1] ) ) continue;
        if ( ! intersection( pts_m[0], pts_m[1], pts_n[1], pts_n[2], isect_mn[0] ) ) continue;
        if ( ! intersection( pts_m[3], pts_m[2], pts_n[1], pts_n[2], isect_mn[1] ) ) continue;

        std::vector<double> vert_nm_n = project_2d_to_1d(isect_nm, pts_m[3]-pts_m[0]);
        std::vector<double> vert_nm_m = project_2d_to_1d(pts_m, pts_m[3]-pts_m[0]);
        std::vector<double> vert_mn_n = project_2d_to_1d(pts_n, pts_n[2]-pts_n[1]);
        std::vector<double> vert_mn_m = project_2d_to_1d(isect_mn, pts_n[2]-pts_n[1]);
        double coords_fact_nm = intersection_1d(vert_nm_n[0],vert_nm_n[1],vert_nm_m[0],vert_nm_m[3])/cv::norm(pts_m[3]-pts_m[0]);
        double coords_fact_mn = intersection_1d(vert_mn_n[1],vert_mn_n[2],vert_mn_m[0],vert_mn_m[1])/cv::norm(pts_n[2]-pts_n[1]);

        /// Compute baseline alignment factors ///
        std::vector<cv::Point2f> bline_n = baseline[n];
        std::vector<cv::Point2f> bline_m = baseline[m];
        if ( ! intersection( bline_n[0], bline_n[1], pts_m[0], pts_m[3], isect_nm[0] ) ) continue;
        if ( ! intersection( bline_m[1], bline_m[0], pts_n[1], pts_n[2], isect_mn[0] ) ) continue;
        double bline_fact_nm = cv::norm( isect_nm[0]-bline_m[0] )/cv::norm( pts_m[3]-pts_m[0] );
        double bline_fact_mn = cv::norm( isect_mn[0]-bline_n[1] )/cv::norm( pts_n[2]-pts_n[1] );

        double coords_fact = 0.5*(coords_fact_nm+coords_fact_mn);
        double bline_fact = 0.5*((1.0-bline_fact_nm)+(1.0-bline_fact_mn));

        double alpha = 0.8;
        double prolong_fact = alpha*bline_fact + (1.0-0.8)*coords_fact;
        if ( prolong_fact < cfg_min_prolong_fact )
          continue;

        /// Add text lines to a line group (new or existing) ///
        std::unordered_set<int> line_group;
        std::vector<int> group_order;
        std::vector<double> group_scores;
        std::vector<double> group_direct;
        int k;
        for ( k=0; k<(int)line_groups.size(); k++ )
          if ( line_groups[k].find(n) != line_groups[k].end() || line_groups[k].find(m) != line_groups[k].end() ) {
            line_group = line_groups[k];
            group_order = line_group_order[k];
            group_scores = line_group_scores[k];
            break;
          }
        line_group.insert(n);
        line_group.insert(m);
        group_order.push_back(n);
        group_order.push_back(m);
        group_scores.push_back(prolong_fact);
        if ( k < (int)line_groups.size() ) {
          line_groups[k] = line_group;
          line_group_order[k] = group_order;
          line_group_scores[k] = group_scores;
          line_group_direct[k] = direct;
        }
        else {
          line_groups.push_back(line_group);
          line_group_order.push_back(group_order);
          line_group_scores.push_back(group_scores);
          line_group_direct.push_back(direct);
        }
      }

  /// Adjust text line order for groups with more than two text lines ///
  std::vector<std::vector<int> > extra_group_order;
  std::vector<double> extra_group_score;

  for ( int k=0; k<(int)line_groups.size(); k++ )
    if ( line_group_scores[k].size() > 1 ) {
      int num_group = line_groups[k].size();

      /// Get horizontal direction ///
      std::vector<int> idx;
      double totlength = 0.0;
      cv::Point2f horiz(0.0,0.0);
      for ( auto it = line_groups[k].begin(); it != line_groups[k].end(); it++ ) {
        idx.push_back(*it);
        totlength += length[*it];
        cv::Point2f tmp = baseline[*it][1]-baseline[*it][0];
        horiz += (length[*it]/cv::norm(tmp))*tmp;
      }
      horiz *= 1.0/totlength;

      /// Check that high horizontal overlaps within group ///
      std::vector<std::vector<double> > blines;
      for ( int j=0; j<(int)idx.size(); j++ )
        blines.push_back( project_2d_to_1d(baseline[idx[j]],horiz) );

      bool recurse = false;
      for ( int j=0; j<(int)blines.size(); j++ )
        for ( int i=j+1; i<(int)blines.size(); i++ ) {
          double iou = IoU_1d(blines[j][0],blines[j][1],blines[i][0],blines[i][1]);
          if ( iou > cfg_max_horiz_iou ) {
            recurse = true;
            goto afterRecourseLoop;
          }
        }
      afterRecourseLoop:

      /// If high overlap recurse with stricter criterion ///
      double recurse_factor = 0.9;
      if ( recurse ) {
        std::vector<xmlNodePtr> recurse_lines;
        std::vector<std::vector<int> > recurse_group_order;
        std::vector<double> recurse_group_score;

        for ( int j=0; j<(int)idx.size(); j++ )
          recurse_lines.push_back(lines[idx[j]]);

        testTextLineContinuation( recurse_lines, recurse_group_order, recurse_group_score, cfg_max_angle_diff*recurse_factor, cfg_max_horiz_iou*recurse_factor, cfg_min_prolong_fact/recurse_factor, fake_baseline );

        if ( recurse_group_order.size() == 0 ) {
          line_groups.erase(line_groups.begin()+k);
          line_group_order.erase(line_group_order.begin()+k);
          line_group_scores.erase(line_group_scores.begin()+k);
          line_group_direct.erase(line_group_direct.begin()+k);
          k--;
        }
        else {
          for ( int j=0; j<(int)recurse_group_order.size(); j++ )
            for ( int i=0; i<(int)recurse_group_order[j].size(); i++ )
              recurse_group_order[j][i] = idx[recurse_group_order[j][i]];
          line_group_order[k] = recurse_group_order[0];
          line_group_scores[k].clear();
          line_group_scores[k].push_back(recurse_group_score[0]);
          for ( int j=1; j<(int)recurse_group_order.size(); j++ ) {
            extra_group_order.push_back(recurse_group_order[j]);
            extra_group_score.push_back(recurse_group_score[j]);
          }
        }

        continue;
      }

      /// Project baseline centers onto the local horizontal axis ///
      std::vector<cv::Point2f> cent;
      for ( int j=0; j<(int)idx.size(); j++ )
        cent.push_back(0.5*(baseline[idx[j]][0]+baseline[idx[j]][1]));
      std::vector<double> hpos = project_2d_to_1d(cent,horiz);

      /// Sort text lines by horizontal center projections ///
      int flags = line_group_direct[k] == 1.0 ? CV_SORT_ASCENDING : CV_SORT_DESCENDING;
      std::vector<int> sidx(num_group);
      cv::sortIdx( hpos, sidx, flags );
      std::vector<int> group_order;
      for ( int j=0; j<(int)sidx.size(); j++ )
        group_order.push_back(idx[sidx[j]]);

      /// Score as average of scores ///
      double score = 0.0;
      for ( int j=0; j<(int)line_group_scores[k].size(); j++ )
        score += line_group_scores[k][j];
      std::vector<double> group_scores;
      group_scores.push_back(score/line_group_scores[k].size());

      line_group_order[k] = group_order;
      line_group_scores[k] = group_scores;
    }

  std::vector<double> line_group_score;
  for ( int k=0; k<(int)line_group_scores.size(); k++ )
    line_group_score.push_back(line_group_scores[k][0]);

  if ( extra_group_order.size() > 0 ) {
    line_group_order.insert(line_group_order.end(), extra_group_order.begin(), extra_group_order.end());
    line_group_score.insert(line_group_score.end(), extra_group_score.begin(), extra_group_score.end());
  }

  _line_group_order = line_group_order;
  _line_group_score = line_group_score;

  return (int)line_groups.size();
}

/**
 * Gets the reading order for a set of text lines (requires single segment polystripe).
 *
 * @param lines                 TextLine elements to process.
 * @param cfg_max_angle_diff    Maximum baseline angle difference for joining.
 * @param cfg_max_horiz_iou     Maximum horizontal IoU for joining.
 * @param cfg_min_prolong_fact  Minimum prolongation factor for joining.
 * @return                      Reading order indices.
 */
std::vector<int> PageXML::getTextLinesReadingOrder( std::vector<xmlNodePt> lines, double cfg_max_angle_diff, double cfg_max_horiz_iou, double cfg_min_prolong_fact, bool fake_baseline ) {
  std::vector<int> reading_order;
  if ( lines.size() == 0 )
    return reading_order;

  /// Get text line join groups ///
  std::vector<std::vector<int> > line_groups;
  std::vector<double> join_group_score;
  int num_joins = testTextLineContinuation( lines, line_groups, join_group_score, cfg_max_angle_diff, cfg_max_horiz_iou, cfg_min_prolong_fact, fake_baseline );

  /// Get points and compute baseline angles and lengths ///
  std::vector<std::vector<cv::Point2f> > baseline;
  std::vector<double> length;
  for ( int n=0; n<(int)lines.size(); n++ ) {
    if ( fake_baseline ) {
      std::vector<cv::Point2f> coords = getPoints(lines[n]);
      std::vector<cv::Point2f> baseline_n;
      baseline_n.push_back(coords[3]);
      baseline_n.push_back(coords[2]);
      baseline.push_back(baseline_n);
    }
    else
      baseline.push_back( getPoints(lines[n],"_:Baseline") );
    length.push_back( getBaselineLength(baseline[n]) );
  }

  /// Get horizontal direction ///
  double totlength = 0.0;
  cv::Point2f horiz(0.0,0.0);
  for ( int n=0; n<(int)lines.size(); n++ ) {
    totlength += length[n];
    cv::Point2f tmp = baseline[n][1]-baseline[n][0];
    horiz += (length[n]/cv::norm(tmp))*tmp;
  }
  horiz *= 1.0/totlength;

  /// Add text lines not in join groups ///
  for ( int n=0; n<(int)lines.size(); n++ ) {
    bool in_join_group = false;
    for ( int i=0; i<num_joins; i++ )
      for ( int j=0; j<(int)line_groups[i].size(); j++ )
        if ( n == line_groups[i][j] ) {
          in_join_group = true;
          i = num_joins;
          break;
        }
    if ( ! in_join_group ) {
      std::vector<int> new_line;
      new_line.push_back(n);
      line_groups.push_back(new_line);
    }
  }

  /// Get vertical group center projections ///
  std::vector<cv::Point2f> cent;
  for ( int i=0; i<(int)line_groups.size(); i++ ) {
    double totlength = 0.0;
    cv::Point2f gcent(0.0,0.0);
    for ( int j=0; j<(int)line_groups[i].size(); j++ ) {
      int n = line_groups[i][j];
      totlength += length[n];
      gcent += length[n]*0.5*(baseline[n][0]+baseline[n][1]);
    }
    gcent *= 1.0/totlength;
    cent.push_back(gcent);
  }
  cv::Point2f vert(-horiz.y,horiz.x);
  std::vector<double> vpos = project_2d_to_1d(cent,vert);

  /// Sort groups by vertical center projections ///
  std::vector<int> sidx(vpos.size());
  cv::sortIdx( vpos, sidx, CV_SORT_ASCENDING );

  /// Populate reading order vector ///
  for ( int ii=0; ii<(int)sidx.size(); ii++ ) {
    int i = sidx[ii];
    for ( int j=0; j<(int)line_groups[i].size(); j++ )
      reading_order.push_back(line_groups[i][j]);
  }

  return reading_order;
}

/**
 * Returns the XML document pointer.
 */
xmlDocPtr PageXML::getDocPtr() {
  return xml;
}
