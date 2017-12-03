/**
 * Header file for the PageXML class
 *
 * @version $Version: 2017.12.03$
 * @copyright Copyright (c) 2016-present, Mauricio Villegas <mauricio_ville@yahoo.com>
 * @license MIT License
 */

#ifndef __PAGEXML_H__
#define __PAGEXML_H__

#include <vector>
#include <chrono>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxslt/transform.h>
#include <opencv2/opencv.hpp>

#if defined (__PAGEXML_LIBCONFIG__)
#include <libconfig.h++>
#endif

#if defined (__PAGEXML_LEPT__)
#include <../leptonica/allheaders.h>
#elif defined (__PAGEXML_MAGICK__)
#include <Magick++.h>
#endif

#if defined (__PAGEXML_OGR__)
#include <ogrsf_frmts.h>
#endif

#if defined (__PAGEXML_LEPT__)
typedef Pix * PageImage;
#elif defined (__PAGEXML_MAGICK__)
typedef Magick::Image PageImage;
#elif defined (__PAGEXML_CVIMG__)
typedef cv::Mat PageImage;
#endif

enum PAGEXML_SETTING {
  PAGEXML_SETTING_INDENT = 0,      // "indent"
  PAGEXML_SETTING_PAGENS,          // "pagens"
  PAGEXML_SETTING_GRAYIMG          // "grayimg"
};

enum PAGEXML_INSERT {
  PAGEXML_INSERT_APPEND = 0,
  PAGEXML_INSERT_PREPEND,
  PAGEXML_INSERT_NEXTSIB,
  PAGEXML_INSERT_PREVSIB
};

enum PAGEXML_READ_DIRECTION {
  PAGEXML_READ_DIRECTION_LTR = 0,
  PAGEXML_READ_DIRECTION_RTL,
  PAGEXML_READ_DIRECTION_TTB,
  PAGEXML_READ_DIRECTION_BTT
};

struct NamedImage {
  std::string id;
  std::string name;
  float rotation = 0.0;
  int direction = 0;
  int x = 0;
  int y = 0;
  PageImage image;
  xmlNodePtr node = NULL;

  NamedImage() {};
  NamedImage( std::string _id,
              std::string _name,
              float _rotation,
              int _direction,
              int _x,
              int _y,
              PageImage _image,
              xmlNodePtr _node
            ) {
    id = _id;
    name = _name;
    rotation = _rotation;
    direction = _direction;
    x = _x;
    y = _y;
    image = _image;
    node = _node;
  }
};

#if defined (__PAGEXML_NOTHROW__)
#define throw_runtime_error( fmt, ... ) fprintf( stderr, "error: " fmt "\n", ##__VA_ARGS__ )
#else
#define throw_runtime_error( fmt, ... ) { char buffer[1024]; snprintf( buffer, sizeof buffer, fmt, ##__VA_ARGS__ ); throw runtime_error(buffer); }
#endif

class PageXML {
  public:
    static const char* settingNames[];
    static char* version();
    static void printVersions( FILE* file = stdout );
    ~PageXML();
    PageXML();
#if defined (__PAGEXML_LIBCONFIG__)
    PageXML( const libconfig::Config& config );
    PageXML( const char* cfgfile );
    void loadConf( const libconfig::Config& config );
#endif
    void printConf( FILE* file = stdout );
    xmlNodePtr newXml( const char* creator, const char* image, const int imgW, const int imgH );
    void loadXml( const char* fname );
    void loadXml( int fnum, bool prevfree = true );
    void loadXmlString( const char* xml_string );
#if defined (__PAGEXML_LEPT__) || defined (__PAGEXML_MAGICK__) || defined (__PAGEXML_CVIMG__)
    void loadImage( int pagenum, const char* fname = NULL, const bool check_size = true );
    void loadImage( xmlNodePtr node, const char* fname = NULL, const bool check_size = true );
#endif
    int simplifyIDs();
    void relativizeImageFilename( const char* xml_path );
    bool areIDsUnique();
    std::vector<NamedImage> crop( const char* xpath, cv::Point2f* margin = NULL, bool opaque_coords = true, const char* transp_xpath = NULL );
    static std::vector<cv::Point2f> stringToPoints( const char* spoints );
    static std::vector<cv::Point2f> stringToPoints( std::string spoints );
    static std::string pointsToString( std::vector<cv::Point2f> points, bool rounded = false );
    static std::string pointsToString( std::vector<cv::Point> points );
    static void pointsLimits( std::vector<cv::Point2f>& points, double& xmin, double& xmax, double& ymin, double& ymax );
    static void pointsBBox( std::vector<cv::Point2f>& points, std::vector<cv::Point2f>& bbox );
    static bool isBBox( const std::vector<cv::Point2f>& points );
    int count( const char* xpath, xmlNodePtr basenode = NULL );
    int count( std::string xpath, xmlNodePtr basenode = NULL );
    std::vector<xmlNodePtr> select( const char* xpath, xmlNodePtr basenode = NULL );
    std::vector<xmlNodePtr> select( std::string xpath, xmlNodePtr basenode = NULL );
    xmlNodePtr selectNth( const char* xpath, unsigned num = 0, xmlNodePtr basenode = NULL );
    xmlNodePtr selectNth( std::string xpath, unsigned num = 0, xmlNodePtr basenode = NULL );
    xmlNodePtr closest( const char* name, xmlNodePtr node );
    static bool nodeIs( xmlNodePtr node, const char* name );
    bool getAttr( const xmlNodePtr node,   const char* name,       std::string& value );
    bool getAttr( const char* xpath,       const char* name,       std::string& value );
    bool getAttr( const std::string xpath, const std::string name, std::string& value );
    int setAttr( std::vector<xmlNodePtr> nodes, const char* name,       const char* value );
    int setAttr( xmlNodePtr node,               const char* name,       const char* value );
    int setAttr( const char* xpath,             const char* name,       const char* value );
    int setAttr( const std::string xpath,       const std::string name, const std::string value );
    xmlNodePtr addElem( const char* name,       const char* id,       const xmlNodePtr node,   PAGEXML_INSERT itype = PAGEXML_INSERT_APPEND, bool checkid = false );
    xmlNodePtr addElem( const char* name,       const char* id,       const char* xpath,       PAGEXML_INSERT itype = PAGEXML_INSERT_APPEND, bool checkid = false );
    xmlNodePtr addElem( const std::string name, const std::string id, const std::string xpath, PAGEXML_INSERT itype = PAGEXML_INSERT_APPEND, bool checkid = false );
    void rmElem( const xmlNodePtr& node );
    int rmElems( const std::vector<xmlNodePtr>& nodes );
    int rmElems( const char* xpath,       xmlNodePtr basenode = NULL );
    int rmElems( const std::string xpath, xmlNodePtr basenode = NULL );
    void setRotation( const xmlNodePtr elem, const float rotation );
    void setReadingDirection( const xmlNodePtr elem, PAGEXML_READ_DIRECTION direction );
    float getRotation( const xmlNodePtr elem );
    int getReadingDirection( const xmlNodePtr elem );
    float getXheight( const xmlNodePtr node );
    float getXheight( const char* id );
    std::vector<cv::Point2f> getPoints( const xmlNodePtr node, const char* xpath = "_:Coords" );
    std::vector<std::vector<cv::Point2f> > getPoints( const std::vector<xmlNodePtr> nodes, const char* xpath = "_:Coords" );
    std::string getTextEquiv( xmlNodePtr node, const char* xpath = ".", const char* separator = " " );
    void processStart( const char* tool, const char* ref = NULL );
    void processEnd();
    void updateLastChange();
    xmlNodePtr setProperty( xmlNodePtr node, const char* key, const char* val = NULL );
    xmlNodePtr setTextEquiv( xmlNodePtr node,   const char* text, const double* _conf = NULL );
    xmlNodePtr setTextEquiv( const char* xpath, const char* text, const double* _conf = NULL );
    xmlNodePtr setCoords( xmlNodePtr node,   const std::vector<cv::Point2f>& points, const double* _conf = NULL );
    xmlNodePtr setCoords( xmlNodePtr node,   const std::vector<cv::Point>& points,   const double* _conf = NULL );
    xmlNodePtr setCoords( const char* xpath, const std::vector<cv::Point2f>& points, const double* _conf = NULL );
    xmlNodePtr setCoordsBBox( xmlNodePtr node, double xmin, double ymin, double width, double height, const double* _conf = NULL );
    xmlNodePtr setBaseline( xmlNodePtr node,   const std::vector<cv::Point2f>& points, const double* _conf = NULL );
    xmlNodePtr setBaseline( const char* xpath, const std::vector<cv::Point2f>& points, const double* _conf = NULL );
    xmlNodePtr setBaseline( xmlNodePtr node, double x1, double y1, double x2, double y2, const double* _conf = NULL );
    xmlNodePtr setPolystripe( xmlNodePtr node, double height, double offset = 0.25 );
    int getPageNumber( xmlNodePtr node );
    void setPageImageOrientation( xmlNodePtr node, int angle, const double* _conf = NULL );
    void setPageImageOrientation( int pagenum,     int angle, const double* _conf = NULL );
    int getPageImageOrientation( xmlNodePtr node );
    int getPageImageOrientation( int pagenum );
    unsigned int getPageWidth( xmlNodePtr node );
    unsigned int getPageWidth( int pagenum );
    unsigned int getPageHeight( xmlNodePtr node );
    unsigned int getPageHeight( int pagenum );
    void setPageImageFilename( xmlNodePtr node, const char* image );
    void setPageImageFilename( int pagenum, const char* image );
    std::string getPageImageFilename( xmlNodePtr node );
    std::string getPageImageFilename( int pagenum );
    PageImage getPageImage( int pagenum );
    PageImage getPageImage( xmlNodePtr node );
    xmlNodePtr addGlyph( xmlNodePtr node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addGlyph( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addWord( xmlNodePtr node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addWord( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addTextLine( xmlNodePtr node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addTextLine( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addTextRegion( xmlNodePtr node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addTextRegion( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addPage( const char* image, const int imgW, const int imgH, const char* id = NULL, xmlNodePtr before_node = NULL );
    xmlNodePtr addPage( std::string image, const int imgW, const int imgH, const char* id = NULL, xmlNodePtr before_node = NULL );
    int write( const char* fname = "-" );
    std::string toString();
#if defined (__PAGEXML_OGR__)
    OGRMultiPolygon* getOGRpolygon( const xmlNodePtr node );
#endif
    xmlDocPtr getDocPtr();
  private:
    bool indent = true;
    bool grayimg = false;
    char* pagens = NULL;
    xmlNsPtr rpagens = NULL;
    std::string xmlDir;
    std::vector<PageImage> pagesImage;
    std::vector<std::string> pagesImageFilename;
    std::vector<std::string> pagesImageBase;
    xmlDocPtr xml = NULL;
    xmlXPathContextPtr context = NULL;
    xsltStylesheetPtr sortattr = NULL;
    xmlNodePtr rootnode = NULL;
    xmlNodePtr process_running = NULL;
    std::chrono::high_resolution_clock::time_point process_started;
    void release();
    void parsePageImage( int pagenum );
    void setupXml();
};

#endif
