/**
 * Header file for the PageXML class
 *
 * @version $Version: 2017.06.09$
 * @copyright Copyright (c) 2016-present, Mauricio Villegas <mauricio_ville@yahoo.com>
 * @license MIT License
 */

#ifndef __PAGEXML_H__
#define __PAGEXML_H__

#include <vector>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <opencv2/opencv.hpp>

#if defined (__PAGEXML_LIBCONFIG__)
#include <libconfig.h++>
#endif

#if defined (__PAGEXML_LEPT__)
#include <../leptonica/allheaders.h>
#elif defined (__PAGEXML_MAGICK__)
#include <Magick++.h>
#endif

enum PAGEXML_SETTING {
  PAGEXML_SETTING_INDENT = 0,      // "indent"
  PAGEXML_SETTING_PAGENS,          // "pagens"
  PAGEXML_SETTING_GRAYIMG,         // "grayimg"
  PAGEXML_SETTING_EXTENDED_NAMES   // "extended_names"
};

enum PAGEXML_INSERT {
  PAGEXML_INSERT_CHILD = 0,
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
#if defined (__PAGEXML_LEPT__)
  Pix *image = NULL;
#elif defined (__PAGEXML_MAGICK__)
  Magick::Image image;
#elif defined (__PAGEXML_CVIMG__)
  cv::Mat image;
#endif
  xmlNodePtr node = NULL;

  NamedImage() {};
  NamedImage( std::string _id,
              std::string _name,
              float _rotation,
              int _direction,
              int _x,
              int _y,
#if defined (__PAGEXML_LEPT__)
              Pix *_image,
#elif defined (__PAGEXML_MAGICK__)
              Magick::Image _image,
#elif defined (__PAGEXML_CVIMG__)
              cv::Mat _image,
#endif
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

class PageXML {
  public:
    static const char* settingNames[];
    static char* version();
    ~PageXML();
    PageXML();
#if defined (__PAGEXML_LIBCONFIG__)
    PageXML( const libconfig::Config& config );
    PageXML( const char* cfgfile );
    void loadConf( const libconfig::Config& config );
#endif
    void printConf( FILE* file );
    void newXml( const char* creator, const char* image, const int imgW = 0, const int imgH = 0 );
    void loadXml( const char* fname );
    void loadXml( int fnum, bool prevfree = true );
#if defined (__PAGEXML_LEPT__) || defined (__PAGEXML_MAGICK__) || defined (__PAGEXML_CVIMG__)
    void loadImage( const char* fname = NULL, const bool check_size = true );
#endif
    int simplifyIDs();
    bool uniqueIDs();
    std::vector<NamedImage> crop( const char* xpath );
    static void stringToPoints( const char* spoints, std::vector<cv::Point2f>& points );
    static void stringToPoints( std::string spoints, std::vector<cv::Point2f>& points );
    static std::string pointsToString( std::vector<cv::Point2f> points, bool rounded = false );
    static std::string pointsToString( std::vector<cv::Point> points );
    static void pointsLimits( std::vector<cv::Point2f>& points, double& xmin, double& xmax, double& ymin, double& ymax );
    static void pointsBBox( std::vector<cv::Point2f>& points, std::vector<cv::Point2f>& bbox );
    static bool isBBox( const std::vector<cv::Point2f>& points );
    std::vector<xmlNodePtr> select( const char* xpath, xmlNodePtr basenode = NULL );
    std::vector<xmlNodePtr> select( std::string xpath, xmlNodePtr basenode = NULL );
    static bool nodeIs( xmlNodePtr node, const char* name );
    bool getAttr( const xmlNodePtr node,   const char* name,       std::string& value );
    bool getAttr( const char* xpath,       const char* name,       std::string& value );
    bool getAttr( const std::string xpath, const std::string name, std::string& value );
    int setAttr( std::vector<xmlNodePtr> nodes, const char* name,       const char* value );
    int setAttr( xmlNodePtr node,               const char* name,       const char* value );
    int setAttr( const char* xpath,             const char* name,       const char* value );
    int setAttr( const std::string xpath,       const std::string name, const std::string value );
    xmlNodePtr addElem( const char* name,       const char* id,       const xmlNodePtr node,   PAGEXML_INSERT itype = PAGEXML_INSERT_CHILD, bool checkid = false );
    xmlNodePtr addElem( const char* name,       const char* id,       const char* xpath,       PAGEXML_INSERT itype = PAGEXML_INSERT_CHILD, bool checkid = false );
    xmlNodePtr addElem( const std::string name, const std::string id, const std::string xpath, PAGEXML_INSERT itype = PAGEXML_INSERT_CHILD, bool checkid = false );
    int rmElems( const std::vector<xmlNodePtr>& nodes );
    int rmElems( const char* xpath,       xmlNodePtr basenode = NULL );
    int rmElems( const std::string xpath, xmlNodePtr basenode = NULL );
    void setRotation( const xmlNodePtr elem, const float rotation );
    void setReadingDirection( const xmlNodePtr elem, PAGEXML_READ_DIRECTION direction );
    float getRotation( const xmlNodePtr elem );
    int getReadingDirection( const xmlNodePtr elem );
    float getXheight( const xmlNodePtr node );
    float getXheight( const char* id );
    bool getFpgram( const xmlNodePtr node, std::vector<cv::Point2f>& fpgram );
    bool getPoints( const xmlNodePtr node, std::vector<cv::Point2f>& points );
    void setLastChange();
    xmlNodePtr setTextEquiv( xmlNodePtr node,   const char* text, const double* _conf = NULL );
    xmlNodePtr setTextEquiv( const char* xpath, const char* text, const double* _conf = NULL );
    xmlNodePtr setCoords( xmlNodePtr node, const std::vector<cv::Point2f>& points );
    xmlNodePtr setCoords( const char* xpath, const std::vector<cv::Point2f>& points );
    xmlNodePtr setBaseline( xmlNodePtr node, const std::vector<cv::Point2f>& points );
    xmlNodePtr setBaseline( const char* xpath, const std::vector<cv::Point2f>& points );
    xmlNodePtr addGlyph( xmlNodePtr node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addGlyph( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addWord( xmlNodePtr node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addWord( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addTextLine( xmlNodePtr node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addTextLine( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addTextRegion( xmlNodePtr node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addTextRegion( const char* id = NULL, const char* before_id = NULL );
    char* getBase();
    int write( const char* fname = "-" );
  private:
    bool indent = true;
    bool grayimg = false;
    bool extended_names = false;
    char* pagens = NULL;
    xmlNsPtr rpagens = NULL;
    char* xmldir = NULL;
    char* imgpath = NULL;
    char* imgbase = NULL;
    xmlDocPtr xml = NULL;
    xmlXPathContextPtr context = NULL;
    xmlNodePtr rootnode = NULL;
#if defined (__PAGEXML_LEPT__)
    Pix *pageimg = NULL;
#elif defined (__PAGEXML_MAGICK__)
    Magick::Image pageimg;
#elif defined (__PAGEXML_CVIMG__)
    cv::Mat pageimg;
#endif
    unsigned int width;
    unsigned int height;
    void release();
    void setupXml();
};

#endif
