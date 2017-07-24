/**
 * Class for input, output and processing of Page XML files and referenced image.
 *
 * @version $Version: 2017.07.17$
 * @copyright Copyright (c) 2016-present, Mauricio Villegas <mauricio_ville@yahoo.com>
 * @license MIT License
 */

#include "PageXML.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdexcept>
#include <regex>

#include <opencv2/opencv.hpp>
#include <libxml/xpathInternals.h>
//#include <libxslt/xslt.h>
//#include <libxslt/xsltconfig.h>

using namespace std;

const char* PageXML::settingNames[] = {
  "indent",
  "pagens",
  "grayimg",
  "extended_names"
};

char default_pagens[] = "http://schema.primaresearch.org/PAGE/gts/pagecontent/2013-07-15";

#ifdef __PAGEXML_MAGICK__
Magick::Color transparent("rgba(0,0,0,0)");
Magick::Color opaque("rgba(0,0,0,100%)");
#endif
regex reXheight(".*x-height: *([0-9.]+) *px;.*");
regex reRotation(".*readingOrientation: *([0-9.]+) *;.*");
regex reDirection(".*readingDirection: *([lrt]t[rlb]) *;.*");

/////////////////////
/// Class version ///
/////////////////////

static char class_version[] = "Version: 2017.07.17";

/**
 * Returns the class version.
 */
char* PageXML::version() {
  return class_version+9;
}

void PageXML::printVersions( FILE* file ) {
  fprintf( file, "compiled against PageXML %s\n", class_version+9 );
  fprintf( file, "compiled against libxml2 %s, linked with %s\n", LIBXML_DOTTED_VERSION, xmlParserVersion );
  //fprintf( file, "compiled against libxslt %s, linked with %s\n", LIBXSLT_DOTTED_VERSION, xsltEngineVersion );
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

#if defined (__PAGEXML_LEPT__)
  if( pageimg != NULL )
    pixDestroy(&pageimg);
#elif defined (__PAGEXML_MAGICK__)
  pageimg = Magick::Image();
#elif defined (__PAGEXML_CVIMG__)
  pageimg = cv::Mat();
#endif
  if( xml != NULL )
    xmlFreeDoc(xml);
  xml = NULL;
  if( context != NULL )
    xmlXPathFreeContext(context);
  context = NULL;
  if( xmldir != NULL )
    free(xmldir);
  xmldir = NULL;
  if( imgpath != NULL )
    free(imgpath);
  imgpath = NULL;
  if( imgbase != NULL )
    free(imgbase);
  imgbase = NULL;
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
  return xmlSaveFormatFileEnc( fname, xml, "utf-8", indent );
}

/**
 * Gets the base name of the Page file derived from the image file name.
 *
 * @return  String containing the image base name.
 */
char* PageXML::getBase() {
  return imgbase;
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
      case PAGEXML_SETTING_EXTENDED_NAMES:
        extended_names = (bool)setting;
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
  fprintf( file, "  extended_names = %s;\n", extended_names ? "true" : "false" );
  fprintf( file, "}\n" );
}


///////////////
/// Loaders ///
///////////////

/**
 * Creates a new Page XML.
 *
 * @param fname  File name of the XML file to read.
 */
void PageXML::newXml( const char* creator, const char* image, const int imgW, const int imgH ) {
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
    loadImage( NULL, false );
#if defined (__PAGEXML_LEPT__)
    width = pixGetWidth(pageimg);
    height = pixGetHeight(pageimg);
#elif defined (__PAGEXML_MAGICK__)
    width = pageimg.columns();
    height = pageimg.rows();
#elif defined (__PAGEXML_CVIMG__)
    width = pageimg.size().width;
    height = pageimg.size().height;
#endif
    setAttr( "//_:Page", "imageWidth", to_string(width).c_str() );
    setAttr( "//_:Page", "imageHeight", to_string(height).c_str() );
#else
    throw runtime_error( "PageXML.newXml: invalid image size" );
#endif
  }
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

  if ( strrchr(fname,'/') != NULL )
    xmldir = strndup(fname,strrchr(fname,'/')-fname);
  FILE *file;
  if ( (file=fopen(fname,"rb")) == NULL )
    throw runtime_error( string("PageXML.loadXml: unable to open file: ") + fname );
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
  setupXml();
}

/**
 * Setups internal variables related to the loaded Page XML.
 */
void PageXML::setupXml() {
  context = xmlXPathNewContext(xml);
  if( context == NULL )
    throw runtime_error( "PageXML.setupXml: unable create xpath context" );
  if( xmlXPathRegisterNs( context, (xmlChar*)"_", (xmlChar*)pagens ) != 0 )
    throw runtime_error( "PageXML.setupXml: unable to register namespace" );
  rootnode = context->node;
  rpagens = xmlSearchNsByHref(xml,xmlDocGetRootElement(xml),(xmlChar*)pagens);

  vector<xmlNodePtr> elem_page = select( "//_:Page" );
  if( elem_page.size() == 0 )
    throw runtime_error( "PageXML.setupXml: unable to find page element" );

  /// Get page size ///
  char* uwidth = (char*)xmlGetProp( elem_page[0], (xmlChar*)"imageWidth" );
  char* uheight = (char*)xmlGetProp( elem_page[0], (xmlChar*)"imageHeight" );
  if( uwidth == NULL || uheight == NULL )
    throw runtime_error( "PageXML.setupXml: problems retrieving page size from xml" );
  width = atoi(uwidth);
  height = atoi(uheight);
  free(uwidth);
  free(uheight);

  /// Get image path ///
  imgpath = (char*)xmlGetProp( elem_page[0], (xmlChar*)"imageFilename" );
  if( imgpath ==NULL )
    throw runtime_error( "PageXML.setupXml: problems retrieving image file from xml" );

  char* p = strrchr(imgpath,'/') == NULL ? imgpath : strrchr(imgpath,'/')+1;
  if( strrchr(p,'.') == NULL )
    throw runtime_error( string("PageXML.setupXml: expected image file name to have an extension: ")+p );
  imgbase = strndup(p,strrchr(p,'.')-p);
  for( char *p=imgbase; *p!='\0'; p++ )
    if( *p == ' ' /*|| *p == '[' || *p == ']' || *p == '(' || *p == ')'*/ )
      *p = '_';

  if( xmldir == NULL )
    xmldir = strdup(".");
}

#if defined (__PAGEXML_LEPT__) || defined (__PAGEXML_MAGICK__) || defined (__PAGEXML_CVIMG__)

/**
 * Loads an image for the Page XML.
 *
 * @param fname  File name of the image to read.
 */
void PageXML::loadImage( const char* fname, const bool check_size ) {
  string aux;
  if( fname == NULL )
    fname = imgpath[0] == '/' ? imgpath : (aux=string(xmldir)+'/'+imgpath).c_str();

#if defined (__PAGEXML_LEPT__)
  pageimg = pixRead(fname);
  if( pageimg == NULL )
    throw runtime_error( string("PageXML.loadImage: problems reading image: ") + fname );
#elif defined (__PAGEXML_MAGICK__)
  try {
    pageimg.read(fname);
  }
  catch( exception& e ) {
    throw runtime_error( string("PageXML.loadImage: problems reading image: ") + e.what() );
  }
#elif defined (__PAGEXML_CVIMG__)
  pageimg = grayimg ? cv::imread(fname,CV_LOAD_IMAGE_GRAYSCALE) : cv::imread(fname);
  if ( ! pageimg.data )
    throw runtime_error( string("PageXML.loadImage: problems reading image: ") + fname );
#endif

  if( grayimg ) {
#if defined (__PAGEXML_LEPT__)
    pageimg = pixRead(fname);
    Pix *orig = pageimg;
    pageimg = pixConvertRGBToGray(orig,0.0,0.0,0.0);
    pixDestroy(&orig);
#elif defined (__PAGEXML_MAGICK__)
    if( pageimg.matte() && pageimg.type() != Magick::GrayscaleMatteType )
      pageimg.type( Magick::GrayscaleMatteType );
    else if( ! pageimg.matte() && pageimg.type() != Magick::GrayscaleType )
      pageimg.type( Magick::GrayscaleType );
#endif
  }

  if( check_size )
#if defined (__PAGEXML_LEPT__)
    if( (int)width != pixGetWidth(pageimg) || (int)height != pixGetHeight(pageimg) )
#elif defined (__PAGEXML_MAGICK__)
    if( width != pageimg.columns() || height != pageimg.rows() )
#elif defined (__PAGEXML_CVIMG__)
    if( (int)width != pageimg.size().width || (int)height != pageimg.size().height )
#endif
      throw runtime_error( string("PageXML.loadImage: discrepancy between image and xml page size: ") + fname );
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
 * @param points   Array of (x,y) coordinates.
 */
void PageXML::stringToPoints( const char* spoints, vector<cv::Point2f>& points ) {
  points.clear();

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
}

/**
 * Parses a string of pairs of coordinates (x1,y1 [x2,y2 ...]) into an array.
 *
 * @param spoints  String containing coordinate pairs.
 * @param points   Array of (x,y) coordinates.
 */
void PageXML::stringToPoints( string spoints, vector<cv::Point2f>& points ) {
  stringToPoints( spoints.c_str(), points );
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
    if( ncontext == NULL )
      throw runtime_error( "PageXML.select: unable create xpath context" );
    if( xmlXPathRegisterNs( ncontext, (xmlChar*)"_", (xmlChar*)pagens ) != 0 )
      throw runtime_error( "PageXML.select: unable to register namespace" );
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

  if( xsel == NULL )
    throw runtime_error( string("PageXML.select: xpath expression failed: ") + xpath );
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
 * Checks if node is of given name.
 *
 * @param node  XML node.
 * @param name  String with name to match against.
 * @return      True if name matches, otherwise false.
 */
bool PageXML::nodeIs( xmlNodePtr node, const char* name ) {
  return xmlStrcmp( node->name, (const xmlChar*)name ) ? false : true;
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

#if defined (__PAGEXML_LEPT__)
  if( pageimg == NULL )
#elif defined (__PAGEXML_MAGICK__)
  if( pageimg.columns() == 0 )
#elif defined (__PAGEXML_CVIMG__)
  if( ! pageimg.data )
#endif
    loadImage();

  for( int n=0; n<(int)elems_coords.size(); n++ ) {
    xmlNodePtr node = elems_coords[n];

    if( xmlStrcmp( node->name, (const xmlChar*)"Coords") )
      throw runtime_error( string("PageXML.crop: expected xpath to match only Coords elements: match=") + to_string(n+1) + " xpath=" + xpath );

    /// Get parent node id ///
    string sampid;
    if( ! getAttr( node->parent, "id", sampid ) )
      throw runtime_error( string("PageXML.crop: expected parent element to include id attribute: match=") + to_string(n+1) + " xpath=" + xpath );

    /// Construct sample name ///
    string sampname = string(".") + sampid;
    if( extended_names )
      if( ! xmlStrcmp( node->parent->name, (const xmlChar*)"TextLine") ||
          ! xmlStrcmp( node->parent->name, (const xmlChar*)"Word") ) {
        string id2;
        getAttr( node->parent->parent, "id", id2 );
        sampname = string(".") + id2 + sampname;
        if( ! xmlStrcmp( node->parent->name, (const xmlChar*)"Word") ) {
          string id3;
          getAttr( node->parent->parent->parent, "id", id3 );
          sampname = string(".") + id3 + sampname;
        }
      }
    sampname = string(imgbase) + sampname;

    /// Get coords points ///
    string spoints;
    if( ! getAttr( node, "points", spoints ) )
      throw runtime_error( string("PageXML.crop: expected a points attribute in Coords element: id=") + sampid );
    vector<cv::Point2f> coords;
    stringToPoints( spoints, coords );

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
    Pix* cropimg = pixClipRectangle(pageimg, box, NULL);
    boxDestroy(&box);
#elif defined (__PAGEXML_MAGICK__)
    Magick::Image cropimg = pageimg;
    cropimg.crop( Magick::Geometry(cropW,cropH,cropX,cropY) );
#elif defined (__PAGEXML_CVIMG__)
    cv::Rect roi;
    roi.x = cropX;
    roi.y = cropY;
    roi.width = cropW;
    roi.height = cropH;
    cv::Mat cropimg = pageimg(roi);
#endif

    if( opaque_coords /*&& ! isBBox( coords )*/ ) {
#if defined (__PAGEXML_LEPT__)
      if( transp_xpath != NULL )
        throw runtime_error( "PageXML.crop: transp_xpath not implemented for __PAGEXML_LEPT__" );
      throw runtime_error( "PageXML.crop: opaque_coords not implemented for __PAGEXML_LEPT__" );

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

      if( transp_xpath != NULL )
        throw runtime_error( "PageXML.crop: transp_xpath not implemented for __PAGEXML_MAGICK__" );

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

          if( ! getAttr( childnode, "points", spoints ) )
            throw runtime_error( string("PageXML.crop: expected a points attribute in Coords element: id=") + childid );
          stringToPoints( spoints, coords );

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
    if( ! attr )
      throw runtime_error( string("PageXML.setAttr: problems setting attribute: name=") + name );
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
  if( ! elem )
    throw runtime_error( string("PageXML.addElem: problems creating new element: name=") + name );
  if( id != NULL ) {
    if( checkid ) {
      vector<xmlNodePtr> idsel = select( (string("//*[@id='")+id+"']").c_str() );
      if( idsel.size() > 0 )
        throw runtime_error( string("PageXML.addElem: id already exists: id=") + id );
    }
    xmlNewProp( elem, (xmlChar*)"id", (xmlChar*)id );
  }

  switch( itype ) {
    case PAGEXML_INSERT_CHILD:
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
  if( target.size() == 0 )
    throw runtime_error( string("PageXML.addElem: unmatched target: xpath=") + xpath );

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
  if( target.size() == 0 )
    throw runtime_error( string("PageXML.addElem: unmatched target: xpath=") + xpath );

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
  else
    throw runtime_error( "PageXML.setRotation: only possible for TextRegion" );
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
  else
    throw runtime_error( "PageXML.setReadingDirection: only possible for TextRegion" );
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
 * Retrieves the features parallelogram for a given TextLine id.
 *
 * @param id     Identifier of the TextLine.
 * @return       Parallelogram, empty list if unset.
 */
bool PageXML::getFpgram( const xmlNodePtr node, vector<cv::Point2f>& fpgram ) {
  if( node == NULL )
    return false;

  vector<xmlNodePtr> coords = select( "_:Coords[@fpgram]", node );
  if( coords.size() == 0 )
    return false;

  string sfpgram;
  if( ! getAttr( coords[0], "fpgram", sfpgram ) )
    return false;

  stringToPoints( sfpgram.c_str(), fpgram );

  return true;
}

/**
 * Retrieves and parses the Coords/@points for a given base node.
 *
 * @param node   Base node.
 * @return       Pointer to the points vector, NULL if unset.
 */
bool PageXML::getPoints( const xmlNodePtr node, vector<cv::Point2f>& points ) {
  if( node == NULL )
    return false;

  vector<xmlNodePtr> coords = select( "_:Coords[@points]", node );
  if( coords.size() == 0 )
    return false;

  string spoints;
  if( ! getAttr( coords[0], "points", spoints ) )
    return false;

  stringToPoints( spoints.c_str(), points );

  return true;
}

/**
 * Sets the LastChange node to the current time.
 */
void PageXML::setLastChange() {
  time_t now;
  time(&now);
  char tstamp[sizeof "YYYY-MM-DDTHH:MM:SSZ"];
  strftime(tstamp, sizeof tstamp, "%FT%TZ", gmtime(&now));

  vector<xmlNodePtr> lastchange = select( "//_:LastChange" );
  if( lastchange.size() != 1 )
    throw runtime_error( "PageXML.setLastChange: unable to select node" );

  rmElems( select( "text()", lastchange[0] ) );

  xmlNodePtr text = xmlNewText( (xmlChar*)tstamp );
  if( ! text || ! xmlAddChild(lastchange[0],text) )
    throw runtime_error( "PageXML.setLastChange: problems updating time stamp" );
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
  if( ! unicode )
    throw runtime_error( "PageXML.setTextEquiv: problems setting TextEquiv" );

  if( _conf != NULL ) {
    char conf[64];
    snprintf( conf, sizeof conf, "%g", *_conf );
    if( ! xmlNewProp( textequiv, (xmlChar*)"conf", (xmlChar*)conf ) )
      throw runtime_error( "PageXML.setTextEquiv: problems setting conf attribute" );
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
xmlNodePtr PageXML::setTextEquiv( const char* xpath, const char* text, const double* conf ) {
  vector<xmlNodePtr> target = select( xpath );
  if( target.size() == 0 )
    throw runtime_error( string("PageXML.setTextEquiv: unmatched target: xpath=") + xpath );

  return setTextEquiv( target[0], text, conf );
}

/**
 * Adds or modifies (if already exists) the Coords for a given node.
 *
 * @param node   The node of element to set the Coords.
 * @param points Vector of x,y coordinates for the points attribute.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setCoords( xmlNodePtr node, const vector<cv::Point2f>& points ) {
  rmElems( select( "_:Coords", node ) );

  xmlNodePtr coords;
  vector<xmlNodePtr> sel = select( "*[local-name()!='Property']", node );
  if( sel.size() > 0 )
    coords = addElem( "Coords", NULL, sel[0], PAGEXML_INSERT_PREVSIB );
  else
    coords = addElem( "Coords", NULL, node );

  if( ! xmlNewProp( coords, (xmlChar*)"points", (xmlChar*)pointsToString(points).c_str() ) )
    throw runtime_error( "PageXML.setCoords: problems setting points attribute" );

  return coords;
}

/**
 * Adds or modifies (if already exists) the Coords for a given xpath.
 *
 * @param node   Selector for element to set the Coords.
 * @param points Vector of x,y coordinates for the points attribute.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setCoords( const char* xpath, const vector<cv::Point2f>& points ) {
  vector<xmlNodePtr> target = select( xpath );
  if( target.size() == 0 )
    throw runtime_error( string("PageXML.setCoords: unmatched target: xpath=") + xpath );

  return setCoords( target[0], points );
}

/**
 * Adds or modifies (if already exists) the Baseline for a given node.
 *
 * @param node   The node of element to set the Baseline.
 * @param points Vector of x,y coordinates for the points attribute.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setBaseline( xmlNodePtr node, const vector<cv::Point2f>& points ) {
  rmElems( select( "_:Baseline", node ) );

  xmlNodePtr baseline;
  vector<xmlNodePtr> sel = select( "*[local-name()!='Property' and local-name()!='Coords']", node );
  if( sel.size() > 0 )
    baseline = addElem( "Baseline", NULL, sel[0], PAGEXML_INSERT_PREVSIB );
  else
    baseline = addElem( "Baseline", NULL, node );

  if( ! xmlNewProp( baseline, (xmlChar*)"points", (xmlChar*)pointsToString(points).c_str() ) )
    throw runtime_error( "PageXML.setBaseline: problems setting points attribute" );

  return baseline;
}

/**
 * Adds or modifies (if already exists) the Baseline for a given xpath.
 *
 * @param xpath  Selector for element to set the Baseline.
 * @param points Vector of x,y coordinates for the points attribute.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setBaseline( const char* xpath, const vector<cv::Point2f>& points ) {
  vector<xmlNodePtr> target = select( xpath );
  if( target.size() == 0 )
    throw runtime_error( string("PageXML.setBaseline: unmatched target: xpath=") + xpath );

  return setBaseline( target[0], points );
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
  xmlNodePtr glyph;

  string gid;
  if( id != NULL )
    gid = string(id);
  else {
    string wid;
    if( ! getAttr( node, "id", wid ) )
      throw runtime_error( "PageXML.addGlyph: expected element to have an id attribute" );
    int n = select( "_:Glyph", node ).size();
    while( true ) {
      if( select( string("*[@id='")+wid+"_g"+to_string(++n)+"']", node ).size() == 0 ) {
        gid = wid+"_g"+to_string(n);
        break;
      }
      if( n > 100000 )
        throw runtime_error( "PageXML.addGlyph: apparently in infinite loop" );
    }
  }

  if( before_id != NULL ) {
    vector<xmlNodePtr> sel = select( string("*[@id='")+before_id+"']", node );
    if( sel.size() == 0 )
      throw runtime_error( string("PageXML.addGlyph: unable to find id=")+before_id );
    glyph = addElem( "Glyph", gid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
  }
  else
    glyph = addElem( "Glyph", gid.c_str(), node, PAGEXML_INSERT_CHILD, true );

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
  if( target.size() == 0 )
    throw runtime_error( string("PageXML.addGlyph: unmatched target: xpath=") + xpath );

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
  xmlNodePtr word;

  string wid;
  if( id != NULL )
    wid = string(id);
  else {
    string lid;
    if( ! getAttr( node, "id", lid ) )
      throw runtime_error( "PageXML.addWord: expected element to have an id attribute" );
    int n = select( "_:Word", node ).size();
    while( true ) {
      if( select( string("*[@id='")+lid+"_w"+to_string(++n)+"']", node ).size() == 0 ) {
        wid = lid+"_w"+to_string(n);
        break;
      }
      if( n > 100000 )
        throw runtime_error( "PageXML.addWord: apparently in infinite loop" );
    }
  }

  if( before_id != NULL ) {
    vector<xmlNodePtr> sel = select( string("*[@id='")+before_id+"']", node );
    if( sel.size() == 0 )
      throw runtime_error( string("PageXML.addWord: unable to find id=")+before_id );
    word = addElem( "Word", wid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
  }
  else
    word = addElem( "Word", wid.c_str(), node, PAGEXML_INSERT_CHILD, true );

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
  if( target.size() == 0 )
    throw runtime_error( string("PageXML.addWord: unmatched target: xpath=") + xpath );

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
  xmlNodePtr textline;

  string lid;
  if( id != NULL )
    lid = string(id);
  else {
    string rid;
    if( ! getAttr( node, "id", rid ) )
      throw runtime_error( "PageXML.addTextLine: expected element to have an id attribute" );
    int n = select( "_:TextLine", node ).size();
    while( true ) {
      if( select( string("*[@id='")+rid+"_l"+to_string(++n)+"']", node ).size() == 0 ) {
        lid = rid+"_l"+to_string(n);
        break;
      }
      if( n > 100000 )
        throw runtime_error( "PageXML.addTextLine: apparently in infinite loop" );
    }
  }

  if( before_id != NULL ) {
    vector<xmlNodePtr> sel = select( string("*[@id='")+before_id+"']", node );
    if( sel.size() == 0 )
      throw runtime_error( string("PageXML.addTextLine: unable to find id=")+before_id );
    textline = addElem( "TextLine", lid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
  }
  else
    textline = addElem( "TextLine", lid.c_str(), node, PAGEXML_INSERT_CHILD, true );

  return textline;
}

/**
 * Adds a TextLine to a given xpath.
 *
 * @param xpath      Selector for element to set the TextLine.
 * @param id         ID for TextLine, if NULL it is selected automatically.
 * @param before_id  If !=NULL inserts it before the TextLine with this ID.
 * @return           Pointer to created element.
 */
xmlNodePtr PageXML::addTextLine( const char* xpath, const char* id, const char* before_id ) {
  vector<xmlNodePtr> target = select( xpath );
  if( target.size() == 0 )
    throw runtime_error( string("PageXML.addTextLine: unmatched target: xpath=") + xpath );

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
  xmlNodePtr textreg;

  string rid;
  if( id != NULL )
    rid = string(id);
  else {
    int n = select( "_:TextRegion", node ).size();
    while( true ) {
      if( select( string("*[@id='t")+to_string(++n)+"']", node ).size() == 0 ) {
        rid = string("t")+to_string(n);
        break;
      }
      if( n > 100000 )
        throw runtime_error( "PageXML.addTextRegion: apparently in infinite loop" );
    }
  }

  if( before_id != NULL ) {
    vector<xmlNodePtr> sel = select( string("*[@id='")+before_id+"']", node );
    if( sel.size() == 0 )
      throw runtime_error( string("PageXML.addTextRegion: unable to find id=")+before_id );
    textreg = addElem( "TextRegion", rid.c_str(), sel[0], PAGEXML_INSERT_PREVSIB, true );
  }
  else
    textreg = addElem( "TextRegion", rid.c_str(), node, PAGEXML_INSERT_CHILD, true );

  return textreg;
}

/**
 * Adds a TextRegion to a given xpath.
 *
 * @param id         ID for TextRegion, if NULL it is selected automatically.
 * @param before_id  If !=NULL inserts it before the TextRegion with this ID.
 * @return           Pointer to created element.
 */
xmlNodePtr PageXML::addTextRegion( const char* id, const char* before_id ) {
  char xpath[] = "//_:Page";
  vector<xmlNodePtr> target = select( xpath );
  if( target.size() == 0 )
    throw runtime_error( string("PageXML.addTextRegion: unmatched target: xpath=") + xpath );

  return addTextRegion( target[0], id, before_id );
}

/**
 * Verifies that all IDs in page are unique.
 */
bool PageXML::uniqueIDs() {
  string id;
  bool unique = true;
  map<string,bool> seen;

  vector<xmlNodePtr> nodes = select( "//*[@id]" );
  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
    getAttr( nodes[n], "id", id );
    if( seen.find(id) != seen.end() && seen[id] ) {
      fprintf( stderr, "PageXML.uniqueIDs: duplicate id: %s\n", id.c_str() );
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
  string sampbase(imgbase);

  vector<xmlNodePtr> nodes = select( "//*[@id][local-name()='TextLine' or local-name()='TextRegion']" );

  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
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

/**
 * Returns the image width.
 */
unsigned int PageXML::getWidth() {
  return width;
}

/**
 * Returns the image height.
 */
unsigned int PageXML::getHeight() {
  return height;
}
