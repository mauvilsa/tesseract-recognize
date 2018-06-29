/**
 * Header file for the PageXML class
 *
 * @version $Version: 2018.06.29$
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

#define xmlNodePt xmlNode*

#if defined (__PAGEXML_LIBCONFIG__)
#include <libconfig.h++>
#endif

#if defined (__PAGEXML_LEPT__)
#include <../leptonica/allheaders.h>
#endif
#if defined (__PAGEXML_MAGICK__)
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

enum PAGEXML_OVERLAP {
  PAGEXML_OVERLAP_COORDS_IOU = 0,
  PAGEXML_OVERLAP_COORDS_IWA,
  PAGEXML_OVERLAP_BASELINE_IWA,
  PAGEXML_OVERLAP_COORDS_BASELINE_IWA
};

struct NamedImage {
  std::string id;
  std::string name;
  float rotation = 0.0;
  int direction = 0;
  int x = 0;
  int y = 0;
  PageImage image;
  xmlNodePt node = NULL;

  NamedImage() {};
  NamedImage( std::string _id,
              std::string _name,
              double _rotation,
              int _direction,
              int _x,
              int _y,
              PageImage _image,
              xmlNodePt _node
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
#else
    PageXML( const char* fname );
#endif
    void printConf( FILE* file = stdout );
    xmlNodePt newXml( const char* creator, const char* image, const int imgW = 0, const int imgH = 0 );
    void loadXml( const char* fname );
    void loadXml( int fnum, bool prevfree = true );
    void loadXmlString( const char* xml_string );
#if defined (__PAGEXML_LEPT__) || defined (__PAGEXML_MAGICK__) || defined (__PAGEXML_CVIMG__)
    void loadImage( int pagenum, const char* fname = NULL, const bool resize_coords = false, int density = 0 );
    void loadImage( xmlNodePt node, const char* fname = NULL, const bool resize_coords = false, int density = 0 );
    void loadImages( const bool resize_coords, const int density );
#endif
    int simplifyIDs();
    void relativizeImageFilename( const char* xml_path );
    std::vector<std::string> getImageBases();
    bool areIDsUnique();
    std::string getNodeName( xmlNodePt node, xmlNodePt base_node = NULL );
    std::vector<NamedImage> crop( const char* xpath, cv::Point2f* margin = NULL, bool opaque_coords = true, const char* transp_xpath = NULL, const char* base_xpath = NULL );
    static std::vector<cv::Point2f> stringToPoints( const char* spoints );
    static std::vector<cv::Point2f> stringToPoints( std::string spoints );
    static std::string pointsToString( std::vector<cv::Point2f> points, bool rounded = false );
    static std::string pointsToString( std::vector<cv::Point> points );
    static void pointsLimits( std::vector<cv::Point2f>& points, double& xmin, double& xmax, double& ymin, double& ymax );
    static void pointsBBox( std::vector<cv::Point2f>& points, std::vector<cv::Point2f>& bbox );
    static bool isBBox( const std::vector<cv::Point2f>& points );
    int count( const char* xpath, xmlNodePt basenode = NULL );
    int count( std::string xpath, xmlNodePt basenode = NULL );
    std::vector<xmlNodePt> select( const char* xpath, const xmlNodePt basenode = NULL );
    std::vector<xmlNodePt> select( std::string xpath, const xmlNodePt node = NULL );
    xmlNodePt selectNth( const char* xpath, unsigned num = 0, const xmlNodePt node = NULL );
    xmlNodePt selectNth( std::string xpath, unsigned num = 0, const xmlNodePt node = NULL );
    xmlNodePt selectByID( const char* id, const xmlNodePt node = NULL );
    xmlNodePt closest( const char* name, xmlNodePt node );
    xmlNodePt parent( const xmlNodePt node );
    static bool nodeIs( xmlNodePt node, const char* name );
    std::string getValue( xmlNodePt node );
    std::string getAttr( const xmlNodePt node,    const char* name );
    std::string getAttr( const char* xpath,       const char* name );
    std::string getAttr( const std::string xpath, const std::string name );
    int setAttr( std::vector<xmlNodePt> nodes,  const char* name,       const char* value );
    int setAttr( const xmlNodePt node,          const char* name,       const char* value );
    int setAttr( const char* xpath,             const char* name,       const char* value );
    int setAttr( const std::string xpath,       const std::string name, const std::string value );
    xmlNodePt insertElem( xmlNodePt elem, const xmlNodePt node, PAGEXML_INSERT itype );
    xmlNodePt addElem( const char* name,       const char* id,       const xmlNodePt node,   PAGEXML_INSERT itype = PAGEXML_INSERT_APPEND, bool checkid = false );
    xmlNodePt addElem( const char* name,       const char* id,       const char* xpath,       PAGEXML_INSERT itype = PAGEXML_INSERT_APPEND, bool checkid = false );
    xmlNodePt addElem( const std::string name, const std::string id, const std::string xpath, PAGEXML_INSERT itype = PAGEXML_INSERT_APPEND, bool checkid = false );
    void rmElem( const xmlNodePt node );
    int rmElems( const std::vector<xmlNodePt>& nodes );
    int rmElems( const char* xpath,       xmlNodePt basenode = NULL );
    int rmElems( const std::string xpath, xmlNodePt basenode = NULL );
    xmlNodePt moveElem( xmlNodePt elem, const xmlNodePt node, PAGEXML_INSERT itype = PAGEXML_INSERT_APPEND );
    int moveElems( const std::vector<xmlNodePt>& elems, const xmlNodePt node, PAGEXML_INSERT itype = PAGEXML_INSERT_APPEND );
    void setRotation( const xmlNodePt elem, const float rotation );
    void setReadingDirection( const xmlNodePt elem, PAGEXML_READ_DIRECTION direction );
    double getBaselineOrientation( xmlNodePt elem );
    double getBaselineOrientation( std::vector<cv::Point2f> points );
    double getBaselineLength( std::vector<cv::Point2f> points );
    double getRotation( const xmlNodePt elem );
    int getReadingDirection( const xmlNodePt elem );
    float getXheight( const xmlNodePt node );
    float getXheight( const char* id );
    std::vector<cv::Point2f> getFpgram( const xmlNodePt node );
    std::vector<cv::Point2f> getPoints( const xmlNodePt node, const char* xpath = "_:Coords" );
    std::vector<std::vector<cv::Point2f> > getPoints( const std::vector<xmlNodePt> nodes, const char* xpath = "_:Coords" );
    std::string getTextEquiv( xmlNodePt node, const char* xpath = ".", const char* separator = " " );
    void processStart( const char* tool, const char* ref = NULL );
    void processEnd();
    void updateLastChange();
    std::string getPropertyValue( xmlNodePt node, const char* key );
    xmlNodePt setProperty( xmlNodePt node, const char* key, const char* val = NULL, const double* _conf = NULL );
    xmlNodePt setProperty( xmlNodePt node, const char* key, const double val, const double* _conf = NULL );
    xmlNodePt setTextEquiv( xmlNodePt node,   const char* text, const double* _conf = NULL );
    xmlNodePt setTextEquiv( const char* xpath, const char* text, const double* _conf = NULL );
    xmlNodePt setCoords( xmlNodePt node,   const std::vector<cv::Point2f>& points, const double* _conf = NULL );
    xmlNodePt setCoords( xmlNodePt node,   const std::vector<cv::Point>& points,   const double* _conf = NULL );
    xmlNodePt setCoords( const char* xpath, const std::vector<cv::Point2f>& points, const double* _conf = NULL );
    xmlNodePt setCoordsBBox( xmlNodePt node, double xmin, double ymin, double width, double height, const double* _conf = NULL );
    xmlNodePt setBaseline( xmlNodePt node,   const std::vector<cv::Point2f>& points, const double* _conf = NULL );
    xmlNodePt setBaseline( const char* xpath, const std::vector<cv::Point2f>& points, const double* _conf = NULL );
    xmlNodePt setBaseline( xmlNodePt node, double x1, double y1, double x2, double y2, const double* _conf = NULL );
    bool intersection( cv::Point2f line1_point1, cv::Point2f line1_point2, cv::Point2f line2_point1, cv::Point2f line2_point2, cv::Point2f& _ipoint );
    bool isPolystripe( std::vector<cv::Point2f> coords, std::vector<cv::Point2f> baseline, double* height = NULL, double* offset = NULL );
    xmlNodePt setPolystripe( xmlNodePt node, double height, double offset = 0.25, bool offset_check = true );
    int getPageNumber( xmlNodePt node );
    void setPageImageOrientation( xmlNodePt node, int angle, const double* _conf = NULL );
    void setPageImageOrientation( int pagenum,     int angle, const double* _conf = NULL );
    int getPageImageOrientation( xmlNodePt node );
    int getPageImageOrientation( int pagenum );
    unsigned int getPageWidth( xmlNodePt node );
    unsigned int getPageWidth( int pagenum );
    unsigned int getPageHeight( xmlNodePt node );
    unsigned int getPageHeight( int pagenum );
    std::vector<cv::Size2i> getPagesSize( std::vector<xmlNodePt> pages );
    std::vector<cv::Size2i> getPagesSize( const char* xpath = "//_:Page" );
    int resize( std::vector<cv::Size2i> sizes, std::vector<xmlNodePt> pages, bool check_aspect_ratio = true );
    int resize( std::vector<cv::Size2i> sizes, const char* xpath = "//_:Page", bool check_aspect_ratio = true );
    int resize( cv::Size2i size, xmlNodePt page, bool check_aspect_ratio = true );
    int resize( double fact, const char* xpath = "//_:Page" );
    void setPageImageFilename( xmlNodePt node, const char* image );
    void setPageImageFilename( int pagenum, const char* image );
    std::string getPageImageFilename( xmlNodePt node );
    std::string getPageImageFilename( int pagenum );
    PageImage getPageImage( int pagenum );
    PageImage getPageImage( xmlNodePt node );
    xmlNodePt addGlyph( xmlNodePt node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePt addGlyph( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePt addWord( xmlNodePt node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePt addWord( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePt addTextLine( xmlNodePt node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePt addTextLine( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePt addTextRegion( xmlNodePt node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePt addTextRegion( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePt addPage( const char* image, const int imgW, const int imgH, const char* id = NULL, xmlNodePt before_node = NULL );
    xmlNodePt addPage( std::string image, const int imgW, const int imgH, const char* id = NULL, xmlNodePt before_node = NULL );
    int write( const char* fname = "-" );
    std::string toString();
#if defined (__PAGEXML_OGR__)
    OGRMultiPolygon* getOGRpolygon( const xmlNodePt node, const char* xpath = "_:Coords" );
    std::vector<OGRMultiPolygon*> getOGRpolygons( std::vector<xmlNodePt> nodes, const char* xpath = "_:Coords" );
    OGRMultiPolygon* getUnionOGRpolygon( std::vector<xmlNodePt> nodes, const char* xpath = "_:Coords" );
    double getOGRpolygonArea( OGRMultiPolygon* poly );
    OGRMultiLineString* getOGRpolyline( const xmlNodePt node, const char* xpath = "_:Baseline" );
    double computeIntersectFactor( OGRMultiPolygon* poly1, OGRMultiPolygon* poly2 );
    double computeIntersectFactor( OGRMultiLineString* poly1, OGRMultiPolygon* poly2 );
    double computeIoU( OGRMultiPolygon* poly1, OGRMultiPolygon* poly2 );
    void computeIoUs( OGRMultiPolygon* poly, std::vector<OGRMultiPolygon*> polys, std::vector<double>& ious );
    void computeCoordsIntersectionsWeightedByArea( xmlNodePtr line, std::vector<xmlNodePtr> regs, std::vector<OGRMultiPolygon*>& reg_polys, std::vector<double>& reg_areas, std::vector<double>& scores );
    void computeBaselineIntersectionsWeightedByArea( xmlNodePtr line, std::vector<xmlNodePtr> regs, std::vector<OGRMultiPolygon*>& reg_polys, std::vector<double>& reg_areas, std::vector<double>& scores );
    int copyTextLinesAssignByOverlap( PageXML& pageFrom, PAGEXML_OVERLAP overlap_type = PAGEXML_OVERLAP_COORDS_IOU, double overlap_fact = 0.5 );
#endif
    int testTextLineContinuation( std::vector<xmlNodePt> lines, std::vector<std::vector<int> >& _line_group_order, std::vector<double>& _line_group_score, double cfg_max_angle_diff = 25*M_PI/180, double cfg_max_horiz_iou = 0.1, double cfg_min_prolong_fact = 0.5, bool fake_baseline = false );
    std::vector<int> getTextLinesReadingOrder( std::vector<xmlNodePt> lines, double cfg_max_angle_diff = 25*M_PI/180, double cfg_max_horiz_iou = 0.1, double cfg_min_prolong_fact = 0.5, bool fake_baseline = false );
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
    xmlNodePt rootnode = NULL;
    xmlNodePt process_running = NULL;
    std::chrono::high_resolution_clock::time_point process_started;
    void release();
    void parsePageImage( int pagenum );
    void setupXml();
};

#endif
