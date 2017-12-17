/**
 * Class for input, output and processing of Page XML files and referenced image.
 *
 * @version $Version: 2017.12.17$
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


/////////////////////
/// Class version ///
/////////////////////

static char class_version[] = "Version: 2017.12.17";

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
xmlNodePtr PageXML::newXml( const char* creator, const char* image, const int imgW, const int imgH ) {
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
 * @param fnum  File number from where to read the XML file.
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
  xmlNodePtr page = selectNth( "//_:Page", pagenum );
  string imageFilename;
  if( ! getAttr( page, "imageFilename", imageFilename ) ) {
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

  vector<xmlNodePtr> elem_page = select( "//_:Page" );
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
 * Loads an image for the Page XML.
 *
 * @param pagenum  The number of the page to load the image.
 * @param fname    File name of the image to read overriding the one in the XML.
 * @param fname    Whether to check that size of image agrees with XML.
 */
void PageXML::loadImage( int pagenum, const char* fname, const bool check_size ) {
  string aux;
  if( fname == NULL ) {
    aux = pagesImageFilename[pagenum].at(0) == '/' ? pagesImageFilename[pagenum] : (xmlDir+'/'+pagesImageFilename[pagenum]);
    fname = aux.c_str();
  }

#if defined (__PAGEXML_LEPT__)
  pagesImage[pagenum] = pixRead(fname);
  if( pagesImage[pagenum] == NULL ) {
    throw_runtime_error( "PageXML.loadImage: problems reading image: %s", fname );
    return;
  }
#elif defined (__PAGEXML_MAGICK__)
  try {
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
    pagesImage[pagenum] = pixRead(fname);
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

  /// Check that image size agrees with XML ///
  if ( check_size ) {
    int width = getPageWidth(pagenum);
    int height = getPageHeight(pagenum);
#if defined (__PAGEXML_LEPT__)
    if( width != pixGetWidth(pagesImage[pagenum]) || height != pixGetHeight(pagesImage[pagenum]) )
#elif defined (__PAGEXML_MAGICK__)
    if( width != (int)pagesImage[pagenum].columns() || height != (int)pagesImage[pagenum].rows() )
#elif defined (__PAGEXML_CVIMG__)
    if( width != pagesImage[pagenum].size().width || height != pagesImage[pagenum].size().height )
#endif
      throw_runtime_error( "PageXML.loadImage: discrepancy between image and xml page size: %s", fname );
  }

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

void PageXML::loadImage( xmlNodePtr node, const char* fname, const bool check_size ) {
  int pagenum = getPageNumber(node);
  if( pagenum >= 0 )
    return loadImage( pagenum, fname, check_size );
  throw_runtime_error( "PageXML.loadImage: node must be a Page or descendant of a Page" );
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
int PageXML::count( const char* xpath, xmlNodePtr basenode ) {
  return select( xpath, basenode ).size();
}

/**
 * Returns number of matched nodes for a given xpath.
 *
 * @param xpath  Selector expression.
 * @param node   XML node for context, set to NULL for root node.
 * @return       Number of matched nodes.
 */
int PageXML::count( string xpath, xmlNodePtr basenode ) {
  return select( xpath.c_str(), basenode ).size();
}

/**
 * Selects nodes given an xpath.
 *
 * @param xpath  Selector expression.
 * @param node   XML node for context, set to NULL for root node.
 * @return       Vector of matched nodes.
 */
vector<xmlNodePtr> PageXML::select( const char* xpath, xmlNodePtr basenode ) {
  vector<xmlNodePtr> matched;

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
    ncontext->node = basenode;
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
vector<xmlNodePtr> PageXML::select( string xpath, xmlNodePtr node ) {
  return select( xpath.c_str(), node );
}

/**
 * Selects the n-th node that matches an xpath.
 *
 * @param xpath  Selector expression.
 * @param xpath  Element number (0-indexed).
 * @param node   XML node for context, set to NULL for root node.
 * @return       Matched node.
 */
xmlNodePtr PageXML::selectNth( const char* xpath, unsigned num, xmlNodePtr node ) {
  vector<xmlNodePtr> matches = select( xpath, node );
  return matches.size() > num ? select( xpath, node )[num] : NULL;
}

/**
 * Selects the n-th node that matches an xpath.
 *
 * @param xpath  Selector expression.
 * @param xpath  Element number (0-indexed).
 * @param node   XML node for context, set to NULL for root node.
 * @return       Matched node.
 */
xmlNodePtr PageXML::selectNth( string xpath, unsigned num, xmlNodePtr node ) {
  return selectNth( xpath.c_str(), num, node );
}

/**
 * Selects closest node of a given name.
 */
xmlNodePtr PageXML::closest( const char* name, xmlNodePtr node ) {
  return selectNth( string("ancestor-or-self::*[local-name()='")+name+"']", 0, node );
}

/**
 * Checks if node is of given name.
 *
 * @param node  XML node.
 * @param name  String with name to match against.
 * @return      True if name matches, otherwise false.
 */
bool PageXML::nodeIs( xmlNodePtr node, const char* name ) {
  return ! node || xmlStrcmp( node->name, (const xmlChar*)name ) ? false : true;
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
vector<NamedImage> PageXML::crop( const char* xpath, cv::Point2f* margin, bool opaque_coords, const char* transp_xpath ) {
  vector<NamedImage> images;

  vector<xmlNodePtr> elems_coords = select( xpath );
  if( elems_coords.size() == 0 )
    return images;

  xmlNodePtr prevPage = NULL;
  string imageBase;
  unsigned int width = 0;
  unsigned int height = 0;
#if defined (__PAGEXML_LEPT__)
  PageImage pageImage = NULL;
#else
  PageImage pageImage;
#endif

  for( int n=0; n<(int)elems_coords.size(); n++ ) {
    xmlNodePtr node = elems_coords[n];

    if( ! nodeIs( node, "Coords") ) {
      throw_runtime_error( "PageXML.crop: expected xpath to match only Coords elements: match=%d xpath=%s", n+1, xpath );
      return images;
    }

    xmlNodePtr page = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
    if( prevPage != page ) {
      prevPage = page;
      int pagenum = getPageNumber(page);
      imageBase = pagesImageBase[pagenum];
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
    string sampid;
    if( ! getAttr( node->parent, "id", sampid ) ) {
      throw_runtime_error( "PageXML.crop: expected parent element to include id attribute: match=%d xpath=%s", n+1, xpath );
      return images;
    }

    /// Construct sample name ///
    string sampname = imageBase + "." + sampid;

    /// Get coords points ///
    string spoints;
    if( ! getAttr( node, "points", spoints ) ) {
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
    cropimg.crop( Magick::Geometry(cropW,cropH,cropX,cropY) );
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
        vector<xmlNodePtr> child_coords = select( transp_xpath, node );

        polys.clear();
        for( int m=0; m<(int)child_coords.size(); m++ ) {
          xmlNodePtr childnode = child_coords[m];

          string childid;
          getAttr( childnode->parent, "id", childid );

          if( ! getAttr( childnode, "points", spoints ) ) {
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

// @todo Get baseline orientation for rotation in case of TextLine with Baseline

    /// Append crop and related data to list ///
    NamedImage namedimage(
      sampid,
      sampname,
      getRotation(node->parent),
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
 * Gets an attribute value from an xml node.
 *
 * @param node   XML node.
 * @param name   Attribute name.
 * @param value  String to set the value.
 * @return       True if attribute found, otherwise false.
*/
bool PageXML::getAttr( const xmlNodePtr node, const char* name, string& value ) {
  if( node == NULL )
    return false;

  xmlChar* attr = xmlGetProp( node, (xmlChar*)name );
  if( attr == NULL )
    return false;
  value = string((char*)attr);
  xmlFree(attr);

  return true;
}

/**
 * Gets an attribute value for a given xpath.
 *
 * @param xpath  Selector for the element to get the attribute.
 * @param name   Attribute name.
 * @param value  String to set the value.
 * @return       True if attribute found, otherwise false.
*/
bool PageXML::getAttr( const char* xpath, const char* name, string& value ) {
  vector<xmlNodePtr> xsel = select( xpath );
  if( xsel.size() == 0 )
    return false;

  return getAttr( xsel[0], name, value );
}

/**
 * Gets an attribute value for a given xpath.
 *
 * @param xpath  Selector for the element to get the attribute.
 * @param name   Attribute name.
 * @param value  String to set the value.
 * @return       True if attribute found, otherwise false.
*/
bool PageXML::getAttr( const string xpath, const string name, string& value ) {
  vector<xmlNodePtr> xsel = select( xpath.c_str() );
  if( xsel.size() == 0 )
    return false;

  return getAttr( xsel[0], name.c_str(), value );
}

/**
 * Adds or modifies (if already exists) an attribute for a given list of nodes.
 *
 * @param nodes  Vector of nodes to set the attribute.
 * @param name   Attribute name.
 * @param value  Attribute value.
 * @return       Number of elements modified.
 */
int PageXML::setAttr( vector<xmlNodePtr> nodes, const char* name, const char* value ) {
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
int PageXML::setAttr( xmlNodePtr node, const char* name, const char* value ) {
  return setAttr( vector<xmlNodePtr>{node}, name, value );
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
 * Creates a new element and adds it relative to a given node.
 *
 * @param name   Name of element to create.
 * @param id     ID attribute for element.
 * @param node   Reference element for insertion.
 * @param itype  Type of insertion.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::addElem( const char* name, const char* id, const xmlNodePtr node, PAGEXML_INSERT itype, bool checkid ) {
  xmlNodePtr elem = xmlNewNode( rpagens, (xmlChar*)name );
  if( ! elem ) {
    throw_runtime_error( "PageXML.addElem: problems creating new element: name=%s", name );
    return NULL;
  }
  if( id != NULL ) {
    if( checkid ) {
      vector<xmlNodePtr> idsel = select( (string("//*[@id='")+id+"']").c_str() );
      if( idsel.size() > 0 ) {
        throw_runtime_error( "PageXML.addElem: id already exists: id=%s", id );
        return NULL;
      }
    }
    xmlNewProp( elem, (xmlChar*)"id", (xmlChar*)id );
  }

  xmlNodePtr sel = NULL;
  switch( itype ) {
    case PAGEXML_INSERT_APPEND:
      elem = xmlAddChild(node,elem);
      break;
    case PAGEXML_INSERT_PREPEND:
      sel = selectNth("*",0,node);
      if( sel )
        elem = xmlAddPrevSibling(sel,elem);
      else
        elem = xmlAddChild(node,elem);
      break;
    case PAGEXML_INSERT_NEXTSIB:
      elem = xmlAddNextSibling(node,elem);
      break;
    case PAGEXML_INSERT_PREVSIB:
      elem = xmlAddPrevSibling(node,elem);
      break;
  }

  return elem;
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
xmlNodePtr PageXML::addElem( const char* name, const char* id, const char* xpath, PAGEXML_INSERT itype, bool checkid ) {
  vector<xmlNodePtr> target = select( xpath );
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
xmlNodePtr PageXML::addElem( const string name, const string id, const string xpath, PAGEXML_INSERT itype, bool checkid ) {
  vector<xmlNodePtr> target = select( xpath );
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
void PageXML::rmElem( const xmlNodePtr& node ) {
  xmlUnlinkNode(node);
  xmlFreeNode(node);
}

/**
 * Removes the elements given in a vector.
 *
 * @param nodes  Vector of elements.
 * @return       Number of elements removed.
 */
int PageXML::rmElems( const vector<xmlNodePtr>& nodes ) {
  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
    xmlUnlinkNode(nodes[n]);
    xmlFreeNode(nodes[n]);
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
int PageXML::rmElems( const char* xpath, xmlNodePtr basenode ) {
  return rmElems( select( xpath, basenode ) );
}

/**
 * Remove the elements that match a given xpath.
 *
 * @param xpath    Selector for elements to remove.
 * @param basenode Base node for element selection.
 * @return         Number of elements removed.
 */
int PageXML::rmElems( const string xpath, xmlNodePtr basenode ) {
  return rmElems( select( xpath.c_str(), basenode ) );
}



/**
 * Sets the rotation angle to a TextRegion node.
 *
 * @param node       Node of the TextRegion element.
 * @param rotation   Rotation angle to set.
 */
void PageXML::setRotation( const xmlNodePtr node, const float rotation ) {
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
void PageXML::setReadingDirection( const xmlNodePtr node, PAGEXML_READ_DIRECTION direction ) {
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
 * Retrieves the rotation angle for a given TextLine or TextRegion node.
 *
 * @param elem   Node of the TextLine or TextRegion element.
 * @return       The rotation angle in degrees, 0 if unset.
 */
float PageXML::getRotation( const xmlNodePtr elem ) {
  float rotation = 0;
  if( elem == NULL )
    return rotation;

  xmlNodePtr node = elem;

  /// If TextLine try to get rotation from custom attribute ///
  if( ! xmlStrcmp( node->name, (const xmlChar*)"TextLine") ) {
    if( ! xmlHasProp( node, (xmlChar*)"custom" ) )
      node = node->parent;
    else {
      xmlChar* attr = xmlGetProp( node, (xmlChar*)"custom" );
      cmatch base_match;
      if( regex_match((char*)attr,base_match,reRotation) )
        rotation = stof(base_match[1].str());
      else
        node = node->parent;
      xmlFree(attr);
    }
  }
  /// Otherwise try to get rotation from readingOrientation attribute ///
  if( xmlHasProp( node, (xmlChar*)"readingOrientation" ) ) {
    xmlChar* attr = xmlGetProp( node, (xmlChar*)"readingOrientation" );
    rotation = stof((char*)attr);
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
int PageXML::getReadingDirection( const xmlNodePtr elem ) {
  int direction = PAGEXML_READ_DIRECTION_LTR;
  if( elem == NULL )
    return direction;

  xmlNodePtr node = elem;

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
float PageXML::getXheight( const xmlNodePtr node ) {
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
  vector<xmlNodePtr> elem = select( string("//*[@id='")+id+"']" );
  return getXheight( elem.size() == 0 ? NULL : elem[0] );
}

/**
 * Retrieves and parses the Coords/@points for a given base node.
 *
 * @param node   Base node.
 * @return       Reference to the points vector.
 */
vector<cv::Point2f> PageXML::getPoints( const xmlNodePtr node, const char* xpath ) {
  vector<cv::Point2f> points;
  if( node == NULL )
    return points;

  vector<xmlNodePtr> coords = select( xpath, node );
  if( coords.size() == 0 )
    return points;

  string spoints;
  if( ! getAttr( coords[0], "points", spoints ) )
    return points;

  return stringToPoints( spoints.c_str() );
}

/**
 * Retrieves and parses the Coords/@points for a given list of base nodes.
 *
 * @param nodes  Base nodes.
 * @return       Reference to the points vector.
 */
std::vector<std::vector<cv::Point2f> > PageXML::getPoints( const std::vector<xmlNodePtr> nodes, const char* xpath ) {
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
std::string PageXML::getTextEquiv( xmlNodePtr node, const char* xpath, const char* separator ) {
  std::vector<xmlNodePtr> nodes = select( std::string(xpath)+"/_:TextEquiv/_:Unicode", node );
  std::string text;
  for ( int n=0; n<(int)nodes.size(); n++ ) {
    xmlChar* t = xmlNodeGetContent(nodes[n]);
    text += std::string(n==0?"":separator) + (char*)t;
    free(t);
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
  xmlNodePtr lastchange = selectNth( "//_:LastChange", 0 );
  if( ! lastchange ) {
    throw_runtime_error( "PageXML.updateLastChange: unable to select node" );
    return;
  }
  rmElems( select( "text()", lastchange ) );
  time_t now;
  time(&now);
  char tstamp[sizeof "YYYY-MM-DDTHH:MM:SSZ"];
  strftime(tstamp, sizeof tstamp, "%FT%TZ", gmtime(&now));
  xmlNodePtr text = xmlNewText( (xmlChar*)tstamp );
  if( ! text || ! xmlAddChild(lastchange,text) ) {
    throw_runtime_error( "PageXML.updateLastChange: problems updating time stamp" );
    return;
  }
}

/**
 * Sets a Property for a given xpath.
 *
 * @param node  The node of element to set the Property.
 * @param key   The key for the Property.
 * @param val   The optional value for the Property.
 * @return      Pointer to created element.
 */
xmlNodePtr PageXML::setProperty( xmlNodePtr node, const char* key, const char* val ) {
  rmElems( select( std::string("_:Property[@key=\"")+key+"\"]", node ) );

  std::vector<xmlNodePtr> siblafter = select( "*[local-name()!='Property' and local-name()!='Metadata']", node );
  std::vector<xmlNodePtr> props = select( "_:Property", node );

  xmlNodePtr prop = NULL;
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
  }
  if ( val != NULL && ! setAttr( prop, "value", val ) ) {
    rmElem( prop );
    throw_runtime_error( "PageXML.setProperty: problems setting value attribute" );
  }

  return prop;
}

/**
 * Adds or modifies (if already exists) the TextEquiv for a given node.
 *
 * @param node   The node of element to set the TextEquiv.
 * @param text   The text string.
 * @param conf   Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setTextEquiv( xmlNodePtr node, const char* text, const double* _conf ) {
  rmElems( select( "_:TextEquiv", node ) );

  xmlNodePtr textequiv = addElem( "TextEquiv", NULL, node );

  xmlNodePtr unicode = xmlNewTextChild( textequiv, NULL, (xmlChar*)"Unicode", (xmlChar*)text );
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
 * @param conf   Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setTextEquiv( const char* xpath, const char* text, const double* _conf ) {
  vector<xmlNodePtr> target = select( xpath );
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
 * @param conf   Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setCoords( xmlNodePtr node, const vector<cv::Point2f>& points, const double* _conf ) {
  rmElems( select( "_:Coords", node ) );

  xmlNodePtr coords;
  vector<xmlNodePtr> sel = select( "*[local-name()!='Property']", node );
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
 * @param conf   Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setCoords( xmlNodePtr node, const vector<cv::Point>& points, const double* _conf ) {
  std::vector<cv::Point2f> points2f;
  cv::Mat(points).convertTo(points2f, cv::Mat(points2f).type());
  return setCoords( node, points2f, _conf );
}

/**
 * Adds or modifies (if already exists) the Coords for a given xpath.
 *
 * @param node   Selector for element to set the Coords.
 * @param points Vector of x,y coordinates for the points attribute.
 * @param conf   Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setCoords( const char* xpath, const vector<cv::Point2f>& points, const double* _conf ) {
  vector<xmlNodePtr> target = select( xpath );
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
 * @param conf   Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setCoordsBBox( xmlNodePtr node, double xmin, double ymin, double width, double height, const double* _conf ) {
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
 * @param conf   Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setBaseline( xmlNodePtr node, const vector<cv::Point2f>& points, const double* _conf ) {
  if( ! nodeIs( node, "TextLine" ) ) {
    throw_runtime_error( "PageXML.setBaseline: node is required to be a TextLine" );
    return NULL;
  }

  rmElems( select( "_:Baseline", node ) );

  xmlNodePtr baseline;
  vector<xmlNodePtr> sel = select( "*[local-name()!='Property' and local-name()!='Coords']", node );
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
 * @param conf   Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setBaseline( const char* xpath, const vector<cv::Point2f>& points, const double* _conf ) {
  vector<xmlNodePtr> target = select( xpath );
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
 * @param conf   Pointer to confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setBaseline( xmlNodePtr node, double x1, double y1, double x2, double y2, const double* _conf ) {
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
 * Sets the Coords of a TextLine as a poly-stripe of the baseline.
 *
 * @param node   The node of element to set the Coords.
 * @param height The height of the poly-stripe in pixels (>0).
 * @param offset The offset of the poly-stripe (>=0 && <= 0.5).
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setPolystripe( xmlNodePtr node, double height, double offset, bool offset_check ) {
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
int PageXML::getPageNumber( xmlNodePtr node ) {
  node = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
  vector<xmlNodePtr> pages = select("//_:Page");
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
 * @param conf   Pointer to confidence value, NULL for no confidence.
 */
void PageXML::setPageImageOrientation( xmlNodePtr node, int angle, const double* _conf ) {
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

  xmlNodePtr orientation = addElem( "ImageOrientation", NULL, node, PAGEXML_INSERT_PREPEND );

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
int PageXML::getPageImageOrientation( xmlNodePtr node ) {
  node = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
  if( ! node ) {
    throw_runtime_error( "PageXML.getPageImageOrientation: node must be a Page or descendant of a Page" );
    return 0;
  }

  node = selectNth( "_:ImageOrientation", 0, node );
  if( ! node )
    return 0;

  string angle;
  getAttr( node, "angle", angle );
  return atoi(angle.c_str());
}
int PageXML::getPageImageOrientation( int pagenum ) {
  return getPageImageOrientation( selectNth("//_:Page",pagenum) );
}

/**
 * Returns the width of a page.
 */
unsigned int PageXML::getPageWidth( xmlNodePtr node ) {
  node = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
  if( ! nodeIs( node, "Page" ) ) {
    throw_runtime_error( "PageXML.getPageWidth: node is required to be a Page or descendant of a Page" );
    return 0;
  }
  string width;
  getAttr( node, "imageWidth", width );
  return atoi(width.c_str());
}
unsigned int PageXML::getPageHeight( int pagenum ) {
  return getPageHeight( selectNth("//_:Page",pagenum) );
}

/**
 * Returns the height of a page.
 */
unsigned int PageXML::getPageHeight( xmlNodePtr node ) {
  node = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
  if( ! nodeIs( node, "Page" ) ) {
    throw_runtime_error( "PageXML.getPageHeight: node is required to be a Page or descendant of a Page" );
    return 0;
  }
  string height;
  getAttr( node, "imageHeight", height );
  return atoi(height.c_str());
}
unsigned int PageXML::getPageWidth( int pagenum ) {
  return getPageWidth( selectNth("//_:Page",pagenum) );
}

/**
 * Sets the imageFilename of a page.
 */
void PageXML::setPageImageFilename( xmlNodePtr node, const char* image ) {
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
string PageXML::getPageImageFilename( xmlNodePtr node ) {
  string image;
  node = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, node );
  if( ! nodeIs( node, "Page" ) ) {
    throw_runtime_error( "PageXML.getPageImageFilename: node is required to be a Page or descendant of a Page" );
    return image;
  }
  getAttr( node, "imageFilename", image );
  return image;
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
PageImage PageXML::getPageImage( xmlNodePtr node ) {
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
xmlNodePtr PageXML::addGlyph( xmlNodePtr node, const char* id, const char* before_id ) {
  if( ! nodeIs( node, "Word" ) ) {
    throw_runtime_error( "PageXML.addGlyph: node is required to be a Word" );
    return NULL;
  }

  xmlNodePtr glyph;

  string gid;
  if( id != NULL )
    gid = string(id);
  else {
    string wid;
    if( ! getAttr( node, "id", wid ) ) {
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
    vector<xmlNodePtr> sel = select( string("*[@id='")+before_id+"']", node );
    if( sel.size() == 0 ) {
      throw_runtime_error( "PageXML.addGlyph: unable to find id=%s", before_id );
      return NULL;
    }
    glyph = addElem( "Glyph", gid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
  }
  else {
    vector<xmlNodePtr> sel = select( "_:TextEquiv", node );
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
xmlNodePtr PageXML::addGlyph( const char* xpath, const char* id, const char* before_id ) {
  vector<xmlNodePtr> target = select( xpath );
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
xmlNodePtr PageXML::addWord( xmlNodePtr node, const char* id, const char* before_id ) {
  if( ! nodeIs( node, "TextLine" ) ) {
    throw_runtime_error( "PageXML.addWord: node is required to be a TextLine" );
    return NULL;
  }

  xmlNodePtr word;

  string wid;
  if( id != NULL )
    wid = string(id);
  else {
    string lid;
    if( ! getAttr( node, "id", lid ) ) {
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
    vector<xmlNodePtr> sel = select( string("*[@id='")+before_id+"']", node );
    if( sel.size() == 0 ) {
      throw_runtime_error( "PageXML.addWord: unable to find id=%s", before_id );
      return NULL;
    }
    word = addElem( "Word", wid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
  }
  else {
    vector<xmlNodePtr> sel = select( "_:TextEquiv", node );
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
xmlNodePtr PageXML::addWord( const char* xpath, const char* id, const char* before_id ) {
  vector<xmlNodePtr> target = select( xpath );
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
xmlNodePtr PageXML::addTextLine( xmlNodePtr node, const char* id, const char* before_id ) {
  if( ! nodeIs( node, "TextRegion" ) ) {
    throw_runtime_error( "PageXML.addTextLine: node is required to be a TextRegion" );
    return NULL;
  }

  xmlNodePtr textline;

  string lid;
  if( id != NULL )
    lid = string(id);
  else {
    string rid;
    if( ! getAttr( node, "id", rid ) ) {
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
    vector<xmlNodePtr> sel = select( string("*[@id='")+before_id+"']", node );
    if( sel.size() == 0 ) {
      throw_runtime_error( "PageXML.addTextLine: unable to find id=%s", before_id );
      return NULL;
    }
    textline = addElem( "TextLine", lid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
  }
  else {
    vector<xmlNodePtr> sel = select( "_:TextEquiv", node );
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
xmlNodePtr PageXML::addTextLine( const char* xpath, const char* id, const char* before_id ) {
  vector<xmlNodePtr> target = select( xpath );
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
xmlNodePtr PageXML::addTextRegion( xmlNodePtr node, const char* id, const char* before_id ) {
  if( ! nodeIs( node, "Page" ) ) {
    throw_runtime_error( "PageXML.addTextRegion: node is required to be a Page" );
    return NULL;
  }

  xmlNodePtr textreg;

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
    vector<xmlNodePtr> sel = select( string("*[@id='")+before_id+"']", node );
    if( sel.size() == 0 ) {
      throw_runtime_error( "PageXML.addTextRegion: unable to find id=%s", before_id );
      return NULL;
    }
    textreg = addElem( "TextRegion", rid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
  }
  else {
    vector<xmlNodePtr> sel = select( "_:TextEquiv", node );
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
xmlNodePtr PageXML::addTextRegion( const char* xpath, const char* id, const char* before_id ) {
  if( xpath == NULL )
    xpath = "//_:Page";
  vector<xmlNodePtr> target = select( xpath );
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
xmlNodePtr PageXML::addPage( const char* image, const int imgW, const int imgH, const char* id, xmlNodePtr before_node ) {
  xmlNodePtr page;

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
    xmlNodePtr pcgts = selectNth("/_:PcGts",0);
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
xmlNodePtr PageXML::addPage( std::string image, const int imgW, const int imgH, const char* id, xmlNodePtr before_node ) {
  return addPage(image.c_str(),imgW,imgH,id,before_node);
}

/**
 * Verifies that all IDs in page are unique.
 */
bool PageXML::areIDsUnique() {
  string id;
  bool unique = true;
  map<string,bool> seen;

  vector<xmlNodePtr> nodes = select( "//*[@id]" );
  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
    getAttr( nodes[n], "id", id );
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
  xmlNodePtr prevPage = NULL;

  vector<xmlNodePtr> nodes = select( "//*[@id][local-name()='TextLine' or local-name()='TextRegion']" );

  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
    xmlNodePtr page = selectNth( "ancestor-or-self::*[local-name()='Page']", 0, nodes[n] );
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

  vector<xmlNodePtr> pages = select( "//_:Page" );

  for ( int n=(int)pages.size()-1; n>=0; n-- ) {
    std::string img;
    getAttr( pages[n], "imageFilename", img );
    if ( img.compare(0, xml_base.length(), xml_base) == 0 ) {
      img.erase(0,xml_base.length());
      setAttr( pages[n], "imageFilename", img.c_str() );
    }
  }
}


#if defined (__PAGEXML_OGR__)

/**
 * Gets the element's Coors as an OGRMultiPolygon.
 *
 * @param node       The element from which to extract the Coords points.
 * @return           Pointer to OGRMultiPolygon element.
 */
OGRMultiPolygon* PageXML::getOGRpolygon( xmlNodePtr node ) {
  std::vector<xmlNodePtr> coords = select( "_:Coords", node );
  if ( coords.size() == 0 )
    return NULL;

  // @todo THE FOLLOWING (BASED ON STRING) IS ONLY TEMPORAL !!!! Should get points and create a polygon point by point
  std::string pts;
  getAttr( coords[0], "points", pts );
  std::replace( pts.begin(), pts.end(), ',', ';');
  std::replace( pts.begin(), pts.end(), ' ', ',');
  std::replace( pts.begin(), pts.end(), ';', ' ');
  std::string::size_type pos = pts.find(',');
  pts = std::string("POLYGON ((")+pts+","+pts.substr(0,pos)+"))";
  //const char *wkt = pts.c_str();
  char *wkt = &pts[0];

  //fprintf(stderr,"%s\n",wkt);

  OGRGeometry *geom;
  OGRGeometryFactory::createFromWkt( &wkt, NULL, &geom );

  return (OGRMultiPolygon*)OGRGeometryFactory::forceToMultiPolygon(geom);
}

#endif

/**
 * Returns the XML document pointer.
 */
xmlDocPtr PageXML::getDocPtr() {
  return xml;
}
