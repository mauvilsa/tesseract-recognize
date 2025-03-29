/**
 * Tool that does layout analysis and OCR using tesseract providing results in Page XML format
 *
 * @author Mauricio Villegas <maurovill+tesseract@gmail.com>
 * @copyright Copyright (c) 2015-present, Mauricio Villegas <maurovill+tesseract@gmail.com>
 * @link https://github.com/mauvilsa/tesseract-recognize
 * @license MIT License
 */

/*** Includes *****************************************************************/
#include <algorithm>
#include <string>
using std::string;
#include <regex>
#include <set>
#include <sstream>
#include <iterator>
#include <getopt.h>

#include <../leptonica/allheaders.h>
#include <../tesseract/baseapi.h>

#include "PageXML.h"

/*** Definitions **************************************************************/
static char tool[] = "tesseract-recognize";
static char version[] = "Version: 2025.03.31";

char gb_page_ns[] = "http://schema.primaresearch.org/PAGE/gts/pagecontent/2013-07-15";

char gb_default_lang[] = "eng";
char gb_default_xpath[] = "//_:TextRegion";
char gb_default_output[] = "-";

char *gb_output = gb_default_output;
char *gb_lang = gb_default_lang;
char *gb_tessdata = NULL;
int gb_psm = tesseract::PSM_AUTO;
int gb_oem = tesseract::OEM_DEFAULT;
bool gb_onlylayout = false;
bool gb_textlevels[] = { false, false, false, false };
bool gb_textatlayout = true;
char *gb_xpath = gb_default_xpath;
char *gb_image = NULL;
int gb_density = 300;
bool gb_inplace = false;

bool gb_save_crops = false;

enum {
  LEVEL_REGION = 0,
  LEVEL_LINE,
  LEVEL_WORD,
  LEVEL_GLYPH
};

const char* levelStrings[] = {
  "region",
  "line",
  "word",
  "glyph"
};

inline static int parseLevel( const char* level ) {
  int levels = sizeof(levelStrings) / sizeof(levelStrings[0]);
  for( int n=0; n<levels; n++ )
    if( ! strcmp(levelStrings[n],level) )
      return n;
  return -1;
}

int gb_layoutlevel = LEVEL_LINE;

enum {
  OPTION_OUTPUT      = 'o',
  OPTION_HELP        = 'h',
  OPTION_VERSION     = 'v',
  OPTION_TESSDATA    = 256,
  OPTION_LANG             ,
  OPTION_LAYOUTLEVEL      ,
  OPTION_TEXTLEVELS       ,
  OPTION_ONLYLAYOUT       ,
  OPTION_SAVECROPS        ,
  OPTION_XPATH            ,
  OPTION_IMAGE            ,
  OPTION_DENSITY          ,
  OPTION_PSM              ,
  OPTION_OEM              ,
  OPTION_INPLACE
};

static char gb_short_options[] = "o:hv";

static struct option gb_long_options[] = {
    { "output",       required_argument, NULL, OPTION_OUTPUT },
    { "help",         no_argument,       NULL, OPTION_HELP },
    { "version",      no_argument,       NULL, OPTION_VERSION },
    { "tessdata",     required_argument, NULL, OPTION_TESSDATA },
    { "lang",         required_argument, NULL, OPTION_LANG },
    { "psm",          required_argument, NULL, OPTION_PSM },
    { "oem",          required_argument, NULL, OPTION_OEM },
    { "layout-level", required_argument, NULL, OPTION_LAYOUTLEVEL },
    { "text-levels",  required_argument, NULL, OPTION_TEXTLEVELS },
    { "only-layout",  no_argument,       NULL, OPTION_ONLYLAYOUT },
    { "save-crops",   no_argument,       NULL, OPTION_SAVECROPS },
    { "xpath",        required_argument, NULL, OPTION_XPATH },
    { "image",        required_argument, NULL, OPTION_IMAGE },
    { "density",      required_argument, NULL, OPTION_DENSITY },
    { "inplace",      no_argument,       NULL, OPTION_INPLACE },
    { 0, 0, 0, 0 }
  };

/*** Functions ****************************************************************/
#define strbool( cond ) ( ( cond ) ? "true" : "false" )

void print_usage() {
  fprintf( stderr, "Description: Layout analysis and OCR using tesseract providing results in Page XML format\n" );
  fprintf( stderr, "Usage: %s [OPTIONS] (IMAGE+|PDF+|PAGEXML)\n", tool );
  fprintf( stderr, "Options:\n" );
  fprintf( stderr, " --lang LANG             Language used for OCR (def.=%s)\n", gb_lang );
  fprintf( stderr, " --tessdata PATH         Location of tessdata (def.=%s)\n", gb_tessdata );
  fprintf( stderr, " --psm MODE              Page segmentation mode (def.=%d)\n", gb_psm );
#if TESSERACT_VERSION >= 0x040000
  fprintf( stderr, " --oem MODE              OCR engine mode (def.=%d)\n", gb_oem );
#endif
  fprintf( stderr, " --layout-level LEVEL    Layout output level: region, line, word, glyph (def.=%s)\n", levelStrings[gb_layoutlevel] );
  fprintf( stderr, " --text-levels L1[,L2]+  Text output level(s): region, line, word, glyph (def.=layout-level)\n" );
  fprintf( stderr, " --only-layout           Only perform layout analysis, no OCR (def.=%s)\n", strbool(gb_onlylayout) );
  fprintf( stderr, " --save-crops            Saves cropped images (def.=%s)\n", strbool(gb_save_crops) );
  fprintf( stderr, " --xpath XPATH           xpath for selecting elements to process (def.=%s)\n", gb_xpath );
  fprintf( stderr, " --image IMAGE           Use given image instead of one in Page XML\n" );
  fprintf( stderr, " --density DENSITY       Density in dpi for pdf rendering (def.=%d)\n", gb_density );
  fprintf( stderr, " --inplace               Overwrite input XML with result (def.=%s)\n", strbool(gb_inplace) );
  fprintf( stderr, " -o, --output            Output page xml file (def.=%s)\n", gb_output );
  fprintf( stderr, " -h, --help              Print this usage information and exit\n" );
  fprintf( stderr, " -v, --version           Print version and exit\n" );
  fprintf( stderr, "\n" );
  int r = system( "tesseract --help-psm 2>&1 | sed '/^ *[02] /d; s| (Default)||;' 1>&2" );
  if( r != 0 )
    fprintf( stderr, "warning: tesseract command not found in path\n" );
#if TESSERACT_VERSION >= 0x040000
  fprintf( stderr, "\n" );
  r += system( "tesseract --help-oem 1>&2" );
#endif
  fprintf( stderr, "Examples:\n" );
  fprintf( stderr, "  %s -o out.xml in1.png in2.png  ### Multiple images as input\n", tool );
  fprintf( stderr, "  %s -o out.xml in.tiff  ### TIFF possibly with multiple frames\n", tool );
  fprintf( stderr, "  %s -o out.xml --density 200 in.pdf\n", tool );
  fprintf( stderr, "  %s -o out.xml --xpath //_:Page in.xml  ### Empty page xml recognize the complete pages\n", tool );
  fprintf( stderr, "  %s -o out.xml --psm 1 in.png  ### Detect page orientation pages\n", tool );
  fprintf( stderr, "  %s -o out.xml --xpath \"//_:TextRegion[@id='r1']\" --layout-level word --only-layout in.xml  ### Detect text lines and words only in TextRegion with id=r1\n", tool );
}


void setCoords( tesseract::ResultIterator* iter, tesseract::PageIteratorLevel iter_level, PageXML& page, xmlNodePtr& xelem, int x, int y, tesseract::Orientation orientation = tesseract::ORIENTATION_PAGE_UP ) {
  int left, top, right, bottom;
  int pagenum = page.getPageNumber(xelem);
  iter->BoundingBox( iter_level, &left, &top, &right, &bottom );
  std::vector<cv::Point2f> points;
  if ( left == 0 && top == 0 && right == (int)page.getPageWidth(pagenum) && bottom == (int)page.getPageHeight(pagenum) )
    points = { cv::Point2f(0,0), cv::Point2f(0,0) };
  else {
    cv::Point2f tl(x+left,y+top);
    cv::Point2f tr(x+right,y+top);
    cv::Point2f br(x+right,y+bottom);
    cv::Point2f bl(x+left,y+bottom);
    switch( orientation ) {
      case tesseract::ORIENTATION_PAGE_UP:    points = { tl, tr, br, bl }; break;
      case tesseract::ORIENTATION_PAGE_RIGHT: points = { tr, br, bl, tl }; break;
      case tesseract::ORIENTATION_PAGE_LEFT:  points = { bl, tl, tr, br }; break;
      case tesseract::ORIENTATION_PAGE_DOWN:  points = { br, bl, tl, tr }; break;
    }
  }
  page.setCoords( xelem, points );
}

void setLineCoords( tesseract::ResultIterator* iter, tesseract::PageIteratorLevel iter_level, PageXML& page, xmlNodePtr& xelem, int x, int y, tesseract::Orientation orientation ) {
  setCoords( iter, iter_level, page, xelem, x, y, orientation );
  std::vector<cv::Point2f> coords = page.getPoints( xelem );
  int x1, y1, x2, y2;
  iter->Baseline( iter_level, &x1, &y1, &x2, &y2 );
  cv::Point2f b_p1(x+x1,y+y1), b_p2(x+x2,y+y2);
  cv::Point2f baseline_p1, baseline_p2;
  if ( ! page.intersection( b_p1, b_p2, coords[0], coords[3], baseline_p1 ) ||
       ! page.intersection( b_p1, b_p2, coords[1], coords[2], baseline_p2 ) ) {
    std::string lid = page.getAttr(xelem,"id");
    fprintf(stderr,"warning: no intersection between baseline and bounding box sides id=%s\n",lid.c_str());
    std::vector<cv::Point2f> baseline = {
      cv::Point2f(x+x1,y+y1),
      cv::Point2f(x+x2,y+y2) };
    page.setBaseline( xelem, baseline );
    return;
  }
  std::vector<cv::Point2f> baseline = { baseline_p1, baseline_p2 };
  page.setBaseline( xelem, baseline );
  double up1 = cv::norm( baseline_p1 - coords[0] );
  double up2 = cv::norm( baseline_p2 - coords[1] );
  double down1 = cv::norm( baseline_p1 - coords[3] );
  double down2 = cv::norm( baseline_p2 - coords[2] );
  double height = 0.5*( up1 + up2 + down1 + down2 );
  double offset = height <= 0.0 ? 0.0 : 0.5*( down1 + down2 ) / height;
  page.setPolystripe( xelem, height <= 0.0 ? 1.0 : height, offset, false );
}

void setTextEquiv( tesseract::ResultIterator* iter, tesseract::PageIteratorLevel iter_level, PageXML& page, xmlNodePtr& xelem ) {
  double conf = 0.01*iter->Confidence( iter_level );
  char* text = iter->GetUTF8Text( iter_level );
  std::string stext(text);
  stext = std::regex_replace( stext, std::regex("^\\s+|\\s+$"), "$1" );
  page.setTextEquiv( xelem, stext.c_str(), &conf );
  delete[] text;
}

template<typename Out>
void split( const std::string &s, char delim, Out result ) {
  std::stringstream ss(s);
  std::string item;
  while( std::getline(ss, item, delim) )
    *(result++) = item;
}

std::set<int> parsePagesSet( std::string range ) {
  std::set<int> pages_set;
  std::vector<std::string> parts;
  split( range, ',', std::back_inserter(parts) );
  for( auto part : parts ) {
    std::string::size_type dash_pos = part.find('-');
    if( dash_pos == std::string::npos )
      pages_set.insert(stoi(part));
    else
      for( int num=stoi(part.substr(0, dash_pos)); num<=stoi(part.substr(dash_pos+1)); num++ )
        pages_set.insert(num);
  }
  return pages_set;
}


/*** Program ******************************************************************/
int main( int argc, char *argv[] ) {

  /// Disable debugging and informational messages from Leptonica. ///
  setMsgSeverity(L_SEVERITY_ERROR);

  /// Parse input arguments ///
  int n,m;
  std::stringstream test;
  std::string token;
  while ( ( n = getopt_long(argc,argv,gb_short_options,gb_long_options,&m) ) != -1 )
    switch ( n ) {
      case OPTION_TESSDATA:
        gb_tessdata = optarg;
        break;
      case OPTION_LANG:
        gb_lang = optarg;
        break;
      case OPTION_PSM:
        gb_psm = atoi(optarg);
        if( gb_psm < tesseract::PSM_AUTO_OSD || gb_psm == tesseract::PSM_AUTO_ONLY || gb_psm >= tesseract::PSM_COUNT ) {
          fprintf( stderr, "%s: error: invalid page segmentation mode: %s\n", tool, optarg );
          return 1;
        }
        break;
#if TESSERACT_VERSION >= 0x040000
      case OPTION_OEM:
        gb_oem = atoi(optarg);
        if( gb_oem < tesseract::OEM_TESSERACT_ONLY || gb_oem >= tesseract::OEM_COUNT ) {
          fprintf( stderr, "%s: error: invalid OCR engine mode: %s\n", tool, optarg );
          return 1;
        }
        break;
#endif
      case OPTION_LAYOUTLEVEL:
        gb_layoutlevel = parseLevel(optarg);
        if( gb_layoutlevel == -1 ) {
          fprintf( stderr, "%s: error: invalid level: %s\n", tool, optarg );
          return 1;
        }
        break;
      case OPTION_TEXTLEVELS:
        test = std::stringstream(optarg);
        while( std::getline(test, token, ',') ) {
          int textlevel = parseLevel(token.c_str());
          if( textlevel == -1 ) {
            fprintf( stderr, "%s: error: invalid level: %s\n", tool, token.c_str() );
            return 1;
          }
          gb_textlevels[textlevel] = true;
          gb_textatlayout = false;
        }
        break;
      case OPTION_ONLYLAYOUT:
        gb_onlylayout = true;
        break;
      case OPTION_SAVECROPS:
        gb_save_crops = true;
        break;
      case OPTION_XPATH:
        gb_xpath = optarg;
        break;
      case OPTION_IMAGE:
        gb_image = optarg;
        break;
      case OPTION_DENSITY:
        gb_density = atoi(optarg);
        break;
      case OPTION_INPLACE:
        gb_inplace = true;
        break;
      case OPTION_OUTPUT:
        gb_output = optarg;
        break;
      case OPTION_HELP:
        print_usage();
        return 0;
      case OPTION_VERSION:
        fprintf( stderr, "%s %s\n", tool, version+9 );
        fprintf( stderr, "compiled against PageXML %s\n", PageXML::version() );
#ifdef TESSERACT_VERSION_STR
        fprintf( stderr, "compiled against tesseract %s, linked with %s\n", TESSERACT_VERSION_STR, tesseract::TessBaseAPI::Version() );
#else
        fprintf( stderr, "linked with tesseract %s\n", tesseract::TessBaseAPI::Version() );
#endif
        return 0;
      default:
        fprintf( stderr, "%s: error: incorrect input argument: %s\n", tool, argv[optind-1] );
        return 1;
    }

  /// Default text level ///
  if ( gb_textatlayout )
    gb_textlevels[gb_layoutlevel] = true;

  /// Check that there is at least one non-option argument ///
  if ( optind >= argc ) {
    fprintf( stderr, "%s: error: at least one input file must be provided, see usage with --help\n", tool );
    return 1;
  }

  /// Initialize tesseract just for layout or with given language and tessdata path///
  tesseract::TessBaseAPI *tessApi = new tesseract::TessBaseAPI();

  if ( gb_onlylayout && gb_psm != tesseract::PSM_AUTO_OSD )
    tessApi->InitForAnalysePage();
  else
#if TESSERACT_VERSION >= 0x040000
  if ( tessApi->Init( gb_tessdata, gb_lang, (tesseract::OcrEngineMode)gb_oem ) ) {
#else
  if ( tessApi->Init( gb_tessdata, gb_lang) ) {
#endif
    fprintf( stderr, "%s: error: could not initialize tesseract\n", tool );
    return 1;
  }

  tessApi->SetPageSegMode( (tesseract::PageSegMode)gb_psm );

  PageXML page;
  int num_pages = 0;
  bool pixRelease = false;
  std::vector<NamedImage> images;
  tesseract::ResultIterator* iter = NULL;

  std::regex reIsXml(".+\\.xml$|^-$",std::regex_constants::icase);
  std::regex reIsTiff(".+\\.tif{1,2}(|\\[[-, 0-9]+\\])$",std::regex_constants::icase);
  std::regex reIsPdf(".+\\.pdf(|\\[[-, 0-9]+\\])$",std::regex_constants::icase);
  std::regex reImagePageNum("(.+)\\[([-, 0-9]+)\\]$");
  std::cmatch base_match;
  char *input_file = argv[optind];
  bool input_xml = std::regex_match(input_file,base_match,reIsXml);

  /// Inplace only when XML input and output not specified ///
  if ( gb_inplace && ( ! input_xml || strcmp(gb_output,"-") ) ) {
    fprintf( stderr, "%s: warning: ignoring --inplace option, output to %s\n", tool, gb_output );
    gb_inplace = false;
  }

  /// Info for process element ///
  char tool_info[128];
  if ( gb_onlylayout )
    snprintf( tool_info, sizeof tool_info, "%s_v%.10s tesseract_v%s", tool, version+9, tesseract::TessBaseAPI::Version() );
  else
    snprintf( tool_info, sizeof tool_info, "%s_v%.10s tesseract_v%s lang=%s", tool, version+9, tesseract::TessBaseAPI::Version(), gb_lang );

  /// Loop through input files ///
  for ( ; optind < argc; optind++ ) {
    input_file = argv[optind];
    input_xml = std::regex_match(input_file,base_match,reIsXml);
    bool input_tiff = std::regex_match(input_file,base_match,reIsTiff);
    bool input_pdf = std::regex_match(input_file,base_match,reIsPdf);

    /// Get selected pages for tiff/pdf if given ///
    std::set<int> pages_set;
    std::string page_sel;
    std::string input_file_str = std::string(input_file);
    if ( input_tiff || input_pdf ) {
      if( std::regex_match(input_file, base_match, reImagePageNum) ) {
        pages_set = parsePagesSet(base_match[2].str());
        page_sel = std::string(base_match[2].str());
        input_file_str = std::string(base_match[1].str());
      }
    }

    /// Input is xml ///
    if ( input_xml ) {
      if ( num_pages > 0 ) {
        fprintf( stderr, "%s: error: only a single page xml allowed as input\n", tool );
        return 1;
      }
      try {
        page.loadXml( input_file ); // if input_file is "-" xml is read from stdin
      } catch ( const std::exception& e ) {
        fprintf( stderr, "%s: error: problems reading xml file: %s\n%s\n", tool, input_file, e.what() );
        return 1;
      }
      if ( gb_image != NULL ) {
        if ( page.count("//_:Page") > 1 ) {
          fprintf( stderr, "%s: error: specifying image with multipage xml input not supported\n", tool );
          return 1;
        }
        page.loadImage( 0, gb_image );
      }
      num_pages += page.count("//_:Page");

      if ( gb_psm == tesseract::PSM_AUTO_OSD && page.count("//_:ImageOrientation") > 0 ) {
        fprintf( stderr, "%s: error: refusing to use OSD on page xml that already contains ImageOrientation elements\n", tool );
        return 1;
      }

      std::vector<xmlNodePtr> sel = page.select(gb_xpath);
      int selPages = 0;
      for ( n=0; n<(int)sel.size(); n++ )
        if ( page.nodeIs( sel[n], "Page" ) )
          selPages++;
      if ( selPages > 0 && selPages != (int)sel.size() ) {
        fprintf( stderr, "%s: error: xpath can select Page or non-Page elements but not a mixture of both: %s\n", tool, gb_xpath );
        return 1;
      }

      if ( selPages == 0 ) {
        pixRelease = true;
        images = page.crop( (std::string(gb_xpath)+"/_:Coords").c_str(), NULL, false );
        page.releaseImages();
      }
      else {
        for ( n=0; n<(int)sel.size(); n++ ) {
          NamedImage namedimage;
          namedimage.image = NULL;
          namedimage.node = sel[n];
          images.push_back( namedimage );
          num_pages++;
        }
      }
    }

    /// Input is tiff image ///
    else if ( input_tiff ) {
      pixRelease = true;

      /// Read input image ///
      PIXA* tiffimage = pixaReadMultipageTiff( input_file_str.c_str() );
      if ( tiffimage == NULL || tiffimage->n == 0 ) {
        fprintf( stderr, "%s: error: problems reading tiff image: %s\n", tool, input_file );
        return 1;
      }

      if ( pages_set.size() > 0 && tiffimage->n <= *pages_set.rbegin() ) {
        fprintf( stderr, "%s: error: invalid page selection (%s) on tiff with %d pages\n", tool, page_sel.c_str(), tiffimage->n+1 );
        return 1;
      }

      for ( n=0; n<tiffimage->n; n++ ) {
        if ( pages_set.size() > 0 && pages_set.find(n) == pages_set.end() )
          continue;

        PageImage image = pixClone(tiffimage->pix[n]);
        std::string pagepath = input_file_str+"["+std::to_string(n)+"]";
        NamedImage namedimage;
        namedimage.image = image;
        if ( num_pages == 0 )
          namedimage.node = page.newXml( tool_info, pagepath.c_str(), pixGetWidth(image), pixGetHeight(image), gb_page_ns );
        else
          namedimage.node = page.addPage( pagepath.c_str(), pixGetWidth(image), pixGetHeight(image) );
        images.push_back( namedimage );
        num_pages++;
      }

      pixaDestroy(&tiffimage);
    }

    /// Input is pdf ///
    else if ( input_pdf ) {
      std::vector< std::pair<double,double> > pdf_pages = gsGetPdfPageSizes(input_file_str);
      if ( pages_set.size() > 0 && (int)pdf_pages.size() <= *pages_set.rbegin() ) {
        fprintf( stderr, "%s: error: invalid page selection (%s) on pdf with %d pages\n", tool, page_sel.c_str(), (int)pdf_pages.size() );
        return 1;
      }

      for ( n=0; n<(int)pdf_pages.size(); n++ ) {
        if ( pages_set.size() > 0 && pages_set.find(n) == pages_set.end() )
          continue;

        std::string pagepath = input_file_str+"["+std::to_string(n)+"]";
        NamedImage namedimage;
        namedimage.image = NULL;
        if ( num_pages == 0 )
          namedimage.node = page.newXml( tool_info, pagepath.c_str(), (int)(0.5+pdf_pages[n].first), (int)(0.5+pdf_pages[n].second), gb_page_ns );
        else
          namedimage.node = page.addPage( pagepath.c_str(), (int)(0.5+pdf_pages[n].first), (int)(0.5+pdf_pages[n].second) );
        images.push_back( namedimage );
        num_pages++;
      }
    }

    /// Input is image ///
    else {
      /// Read input image ///
      PageImage image = pixRead( input_file );
      if ( image == NULL ) {
        fprintf( stderr, "%s: error: problems reading image: %s\n", tool, input_file );
        return 1;
      }

      NamedImage namedimage;
      namedimage.image = NULL;
      if ( num_pages == 0 )
        namedimage.node = page.newXml( tool_info, input_file, pixGetWidth(image), pixGetHeight(image), gb_page_ns );
      else
        namedimage.node = page.addPage( input_file, pixGetWidth(image), pixGetHeight(image) );
      num_pages++;
      pixDestroy(&image);
      images.push_back( namedimage );
    }
  }

  page.processStart(tool_info);

  /// Loop through all images to process ///
  for ( n=0; n<(int)images.size(); n++ ) {
    xmlNodePtr xpg = page.closest( "Page", images[n].node );

    if ( images[n].image == NULL ) {
      try {
        page.loadImage(xpg, NULL, true, gb_density );
        images[n].image = page.getPageImage(n);
      } catch ( const std::exception& e ) {
        fprintf( stderr, "%s: error: problems loading page image: %s :: %s\n", tool, page.getPageImageFilename(n).c_str(), e.what() );
        return 1;
      }
    }

    tessApi->SetImage( images[n].image );
    if ( gb_save_crops && input_xml ) {
      std::string fout = std::string("crop_")+std::to_string(n)+"_"+images[n].id+".png";
      fprintf( stderr, "%s: writing cropped image: %s\n", tool, fout.c_str() );
      pixWriteImpliedFormat( fout.c_str(), images[n].image, 0, 0 );
    }

    /// For xml input setup node level ///
    xmlNodePtr node = NULL;
    int node_level = -1;
    if ( input_xml ) {
      node = images[n].node->parent;
      if ( page.nodeIs( node, "TextRegion" ) )
        node_level = LEVEL_REGION;
      else if ( page.nodeIs( node, "TextLine" ) ) {
        node_level = LEVEL_LINE;
        if ( gb_psm != tesseract::PSM_SINGLE_LINE && gb_psm != tesseract::PSM_RAW_LINE ) {
          fprintf( stderr, "%s: error: for xml input selecting text lines, valid page segmentation modes are %d and %d\n", tool, tesseract::PSM_SINGLE_LINE, tesseract::PSM_RAW_LINE );
          return 1;
        }
      }
      else if ( page.nodeIs( node, "Word" ) ) {
        node_level = LEVEL_WORD;
        if ( gb_psm != tesseract::PSM_SINGLE_WORD && gb_psm != tesseract::PSM_CIRCLE_WORD ) {
          fprintf( stderr, "%s: error: for xml input selecting words, valid page segmentation modes are %d and %d\n", tool, tesseract::PSM_SINGLE_WORD, tesseract::PSM_CIRCLE_WORD );
          return 1;
        }
      }
      else if ( page.nodeIs( node, "Glyph" ) ) {
        node_level = LEVEL_GLYPH;
        if ( gb_psm != tesseract::PSM_SINGLE_CHAR ) {
          fprintf( stderr, "%s: error: for xml input selecting glyphs, the only valid page segmentation mode is %d\n", tool, tesseract::PSM_SINGLE_CHAR );
          return 1;
        }
      }
      if ( gb_layoutlevel < node_level ) {
        fprintf( stderr, "%s: error: layout level lower than xpath selection level\n", tool );
        return 1;
      }
    }

    /// Perform layout analysis ///
    if ( gb_onlylayout && gb_psm != tesseract::PSM_AUTO_OSD )
      iter = (tesseract::ResultIterator*)( tessApi->AnalyseLayout() );

    /// Perform recognition ///
    else {
      tessApi->Recognize( 0 );
      iter = tessApi->GetIterator();
    }

    if ( iter != NULL && ! iter->Empty( tesseract::RIL_BLOCK ) ) {
      /// Orientation and Script Detection ///
      tesseract::Orientation orientation;
      tesseract::WritingDirection writing_direction;
      tesseract::TextlineOrder textline_order;
      float deskew_angle;
      iter->Orientation( &orientation, &writing_direction, &textline_order, &deskew_angle );

      if ( gb_psm == tesseract::PSM_AUTO_OSD ) {
        if ( deskew_angle != 0.0 )
          page.setProperty( xpg, "deskewAngle", deskew_angle );
        switch ( orientation ) {
          case tesseract::ORIENTATION_PAGE_RIGHT:          page.setProperty( xpg, "apply-image-orientation", -90 );      break;
          case tesseract::ORIENTATION_PAGE_LEFT:           page.setProperty( xpg, "apply-image-orientation", 90 );       break;
          case tesseract::ORIENTATION_PAGE_DOWN:           page.setProperty( xpg, "apply-image-orientation", 180 );      break;
          default: break;
        }
        switch ( writing_direction ) {
          case tesseract::WRITING_DIRECTION_LEFT_TO_RIGHT: page.setProperty( xpg, "readingDirection", "left-to-right" ); break;
          case tesseract::WRITING_DIRECTION_RIGHT_TO_LEFT: page.setProperty( xpg, "readingDirection", "right-to-left" ); break;
          case tesseract::WRITING_DIRECTION_TOP_TO_BOTTOM: page.setProperty( xpg, "readingDirection", "top-to-bottom" ); break;
        }
        switch ( textline_order ) {
          case tesseract::TEXTLINE_ORDER_LEFT_TO_RIGHT:    page.setProperty( xpg, "textLineOrder", "left-to-right" );    break;
          case tesseract::TEXTLINE_ORDER_RIGHT_TO_LEFT:    page.setProperty( xpg, "textLineOrder", "right-to-left" );    break;
          case tesseract::TEXTLINE_ORDER_TOP_TO_BOTTOM:    page.setProperty( xpg, "textLineOrder", "top-to-bottom" );    break;
        }
      }

      /// Loop through blocks ///
      int block = 0;
      while ( gb_layoutlevel >= LEVEL_REGION ) {
        /// Skip non-text blocks ///
        /*
         0 PT_UNKNOWN,        // Type is not yet known. Keep as the first element.
         1 PT_FLOWING_TEXT,   // Text that lives inside a column.
         2 PT_HEADING_TEXT,   // Text that spans more than one column.
         3 PT_PULLOUT_TEXT,   // Text that is in a cross-column pull-out region.
         4 PT_EQUATION,       // Partition belonging to an equation region.
         5 PT_INLINE_EQUATION,  // Partition has inline equation.
         6 PT_TABLE,          // Partition belonging to a table region.
         7 PT_VERTICAL_TEXT,  // Text-line runs vertically.
         8 PT_CAPTION_TEXT,   // Text that belongs to an image.
         9 PT_FLOWING_IMAGE,  // Image that lives inside a column.
         10 PT_HEADING_IMAGE,  // Image that spans more than one column.
         11 PT_PULLOUT_IMAGE,  // Image that is in a cross-column pull-out region.
         12 PT_HORZ_LINE,      // Horizontal Line.
         13 PT_VERT_LINE,      // Vertical Line.
         14 PT_NOISE,          // Lies outside of any column.
        */
        if ( iter->BlockType() > tesseract::PT_CAPTION_TEXT ) {
          if ( ! iter->Next( tesseract::RIL_BLOCK ) )
            break;
          continue;
        }

        block++;

        xmlNodePtr xreg = NULL;
        std::string rid = "b" + std::to_string(block);

        /// If xml input and region selected, prepend id to rid and set xreg to node ///
        if ( node_level == LEVEL_REGION ) {
          rid = std::string(images[n].id) + "_" + rid;
          xreg = node;
        }

        /// If it is multipage, prepend page number to rid ///
        if ( num_pages > 1 )
          rid = std::string("pg") + std::to_string(1+page.getPageNumber(xpg)) + "_" + rid;

        /// Otherwise add block as TextRegion element ///
        if ( node_level < LEVEL_REGION ) {
          xreg = page.addTextRegion( xpg, rid.c_str() );

          /// Set block bounding box and text ///
          setCoords( iter, tesseract::RIL_BLOCK, page, xreg, images[n].x, images[n].y );
          if ( ! gb_onlylayout && gb_textlevels[LEVEL_REGION] )
            setTextEquiv( iter, tesseract::RIL_BLOCK, page, xreg );
        }

        /// Set rotation and reading direction ///
        /*tesseract::Orientation orientation;
        tesseract::WritingDirection writing_direction;
        tesseract::TextlineOrder textline_order;
        float deskew_angle;*/
        iter->Orientation( &orientation, &writing_direction, &textline_order, &deskew_angle );
        if ( ! input_xml || node_level <= LEVEL_REGION ) {
          if ( deskew_angle != 0.0 )
            page.setProperty( xpg, "deskewAngle", deskew_angle );
          PAGEXML_READ_DIRECTION direct = PAGEXML_READ_DIRECTION_LTR;
          switch( writing_direction ) {
            case tesseract::WRITING_DIRECTION_LEFT_TO_RIGHT: direct = PAGEXML_READ_DIRECTION_LTR; break;
            case tesseract::WRITING_DIRECTION_RIGHT_TO_LEFT: direct = PAGEXML_READ_DIRECTION_RTL; break;
            case tesseract::WRITING_DIRECTION_TOP_TO_BOTTOM: direct = PAGEXML_READ_DIRECTION_TTB; break;
          }
          page.setReadingDirection( xreg, direct );
          /*float orient = 0.0;
          switch( orientation ) {
            case tesseract::ORIENTATION_PAGE_UP:    orient = 0.0;   break;
            case tesseract::ORIENTATION_PAGE_RIGHT: orient = -90.0; break;
            case tesseract::ORIENTATION_PAGE_LEFT:  orient = 90.0;  break;
            case tesseract::ORIENTATION_PAGE_DOWN:  orient = 180.0; break;
          }
          page.setRotation( xreg, orient );*/
        }

        /// Loop through paragraphs in current block ///
        int para = 0;
        while ( gb_layoutlevel >= LEVEL_REGION ) {
          para++;

          /// Loop through lines in current paragraph ///
          int line = 0;
          while ( gb_layoutlevel >= LEVEL_LINE ) {
            line++;

            xmlNodePtr xline = NULL;

            /// If xml input and line selected, set xline to node ///
            if ( node_level == LEVEL_LINE )
              xline = node;

            /// Otherwise add TextLine element ///
            else if ( node_level < LEVEL_LINE ) {
              std::string lid = rid + "_p" + std::to_string(para) + "_l" + std::to_string(line);
              xline = page.addTextLine( xreg, lid.c_str() );
            }

            /// Set line bounding box, baseline and text ///
            if ( xline != NULL ) {
              setLineCoords( iter, tesseract::RIL_TEXTLINE, page, xline, images[n].x, images[n].y, orientation );
              if ( ! gb_onlylayout && gb_textlevels[LEVEL_LINE] )
                setTextEquiv( iter, tesseract::RIL_TEXTLINE, page, xline );
            }

            /// Loop through words in current text line ///
            while ( gb_layoutlevel >= LEVEL_WORD ) {
              xmlNodePtr xword = NULL;

              /// If xml input and word selected, set xword to node ///
              if ( node_level == LEVEL_WORD )
                xword = node;

              /// Otherwise add Word element ///
              else if ( node_level < LEVEL_WORD )
                xword = page.addWord( xline );

              /// Set word bounding box and text ///
              if ( xword != NULL ) {
                setCoords( iter, tesseract::RIL_WORD, page, xword, images[n].x, images[n].y, orientation );
                if ( ! gb_onlylayout && gb_textlevels[LEVEL_WORD] )
                  setTextEquiv( iter, tesseract::RIL_WORD, page, xword );
              }

              /// Loop through symbols in current word ///
              while ( gb_layoutlevel >= LEVEL_GLYPH ) {
                /// Set xglyph to node or add new Glyph element depending on the case ///
                xmlNodePtr xglyph = node_level == LEVEL_GLYPH ? node : page.addGlyph( xword );

                /// Set symbol bounding box and text ///
                setCoords( iter, tesseract::RIL_SYMBOL, page, xglyph, images[n].x, images[n].y, orientation );
                if ( ! gb_onlylayout && gb_textlevels[LEVEL_GLYPH] )
                  setTextEquiv( iter, tesseract::RIL_SYMBOL, page, xglyph );

                if ( iter->IsAtFinalElement( tesseract::RIL_WORD, tesseract::RIL_SYMBOL ) )
                  break;
                iter->Next( tesseract::RIL_SYMBOL );
              } // while ( gb_layoutlevel >= LEVEL_GLYPH ) {

              if ( iter->IsAtFinalElement( tesseract::RIL_TEXTLINE, tesseract::RIL_WORD ) )
                break;
              iter->Next( tesseract::RIL_WORD );
            } // while ( gb_layoutlevel >= LEVEL_WORD ) {

            if ( iter->IsAtFinalElement( tesseract::RIL_PARA, tesseract::RIL_TEXTLINE ) )
              break;
            iter->Next( tesseract::RIL_TEXTLINE );
          } // while ( gb_layoutlevel >= LEVEL_LINE ) {

          if ( iter->IsAtFinalElement( tesseract::RIL_BLOCK, tesseract::RIL_PARA ) )
            break;
          iter->Next( tesseract::RIL_PARA );
        } // while ( gb_layoutlevel >= LEVEL_REGION ) {

        if ( ! iter->Next( tesseract::RIL_BLOCK ) )
          break;
      } // while ( gb_layoutlevel >= LEVEL_REGION ) {
    } // if ( iter != NULL && ! iter->Empty( tesseract::RIL_BLOCK ) ) {
    page.releaseImage(xpg);
  } // for ( n=0; n<(int)images.size(); n++ ) {

  /// Apply image orientations ///
  std::vector<xmlNodePtr> sel = page.select("//_:Page[_:Property/@key='apply-image-orientation']");
  for ( n=(int)sel.size()-1; n>=0; n-- ) {
    int angle = atoi( page.getPropertyValue( sel[n], "apply-image-orientation" ).c_str() );
    if ( angle )
      page.rotatePage( -angle, sel[n], true );
    page.rmElems( page.select("_:Property[@key='apply-image-orientation']", sel[n]) );
    std::vector<xmlNodePtr> lines = page.select(".//_:TextLine",sel[n]);
    /// Fix image orientation using baselines ///
    if ( lines.size() > 0 ) {
      double domangle = page.getDominantBaselinesOrientation(lines);
      angle = 0;
      if ( domangle >= M_PI/4 && domangle < 3*M_PI/4 )
        angle = -90;
      else if ( domangle <= -M_PI/4 && domangle > -3*M_PI/4 )
        angle = 90;
      else if ( domangle >= 3*M_PI/4 || domangle <= -3*M_PI/4 )
        angle = 180;
      if ( angle )
        page.rotatePage(angle, sel[n], true);
    }
  }

  /// Fill in "0,0 0,0" Word Coords ///
  sel = page.select("//_:Word[_:Coords/@points='0,0 0,0']");
  for ( n=(int)sel.size()-1; n>=0; n-- ) {
    xmlNodePtr elem = sel[n];
    xmlNodePtr elem_pre = page.selectNth("preceding-sibling::_:Word[_:Coords/@points!='0,0 0,0']", -1, elem);
    xmlNodePtr elem_fol = page.selectNth("following-sibling::_:Word[_:Coords/@points!='0,0 0,0']", 0, elem);
    if ( elem_pre == NULL && elem_fol == NULL ) {
      page.setCoords(elem, page.getPoints(page.parent(elem)));
      page.setProperty(elem, "coords-unk-filler");
      continue;
    }
    std::vector<cv::Point2f> pts_pre = page.getPoints(elem_pre);
    std::vector<cv::Point2f> pts_fol = page.getPoints(elem_fol);
    std::vector<cv::Point2f> pts;
    if ( elem_pre != NULL && elem_fol != NULL ) {
      pts.push_back(pts_pre[1]);
      pts.push_back(pts_fol[0]);
      pts.push_back(pts_fol[3]);
      pts.push_back(pts_pre[2]);
    }
    else if ( elem_pre != NULL ) {
      cv::Point2f upper = pts_pre[1] - pts_pre[0];
      cv::Point2f lower = pts_pre[2] - pts_pre[3];
      upper = upper/cv::norm(upper) + pts_pre[1];
      lower = lower/cv::norm(lower) + pts_pre[2];
      pts.push_back(pts_pre[1]);
      pts.push_back(upper);
      pts.push_back(lower);
      pts.push_back(pts_pre[2]);
    }
    else {
      cv::Point2f upper = pts_fol[0] - pts_fol[1];
      cv::Point2f lower = pts_fol[3] - pts_fol[2];
      upper = upper/cv::norm(upper) + pts_fol[0];
      lower = lower/cv::norm(lower) + pts_fol[3];
      pts.push_back(upper);
      pts.push_back(pts_fol[0]);
      pts.push_back(pts_fol[3]);
      pts.push_back(lower);
    }
    page.setCoords(elem, pts);
    page.setProperty(elem, "coords-unk-filler");
  }

  /// Try to make imageFilename be a relative path w.r.t. the output XML ///
  if ( ! input_xml && ! gb_inplace && strcmp(gb_output,"-") )
    page.relativizeImageFilename(gb_output);

  /// Write resulting XML ///
  int bytes = page.write( gb_inplace ? input_file : gb_output );
  if ( bytes <= 0 )
    fprintf( stderr, "%s: error: problems writing to output xml\n", tool );

  /// Release resources ///
  if ( pixRelease )
    for ( n=0; n<(int)images.size(); n++ )
      pixDestroy(&(images[n].image));
  tessApi->End();
  delete tessApi;
  delete iter;

  return bytes <= 0 ? 1 : 0;
}
