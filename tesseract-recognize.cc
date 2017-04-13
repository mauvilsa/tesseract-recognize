/**
 * Tool that does layout anaysis and/or text recognition using tesseract using Page XML format
 *
 * @version $Version: 2017.04.13$
 * @author Mauricio Villegas <mauvilsa@upv.es>
 * @copyright Copyright (c) 2015-present, Mauricio Villegas <mauvilsa@upv.es>
 * @link https://github.com/mauvilsa/tesseract-recognize
 * @license MIT License
 */

/*** Includes *****************************************************************/
#include <algorithm>
#include <string>
#include <regex>
#include <getopt.h>
#include <time.h>

#include <../leptonica/allheaders.h>
#include <../tesseract/baseapi.h>

#include "PageXML.h"

/*** Definitions **************************************************************/
static char tool[] = "tesseract-recognize";
static char version[] = "$Version: 2017.04.13$";

char gb_default_lang[] = "eng";
char gb_default_xpath[] = "//_:TextRegion";

char *gb_lang = gb_default_lang;
char *gb_tessdata = NULL;
int gb_psm = tesseract::PSM_AUTO;
int gb_oem = tesseract::OEM_DEFAULT;
bool gb_onlylayout = false;
bool gb_textlevels[] = { false, false, false, false };
bool gb_textatlayout = true;
char *gb_xpath = gb_default_xpath;

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
  OPTION_HELP        = 'h',
  OPTION_VERSION     = 'v',
  OPTION_TESSDATA    = 256,
  OPTION_LANG             ,
  OPTION_LAYOUTLEVEL      ,
  OPTION_TEXTLEVELS       ,
  OPTION_ONLYLAYOUT       ,
  OPTION_XPATH            ,
  OPTION_PSM              ,
  OPTION_OEM
};

static char gb_short_options[] = "hv";

static struct option gb_long_options[] = {
    { "help",         no_argument,       NULL, OPTION_HELP },
    { "version",      no_argument,       NULL, OPTION_VERSION },
    { "tessdata",     required_argument, NULL, OPTION_TESSDATA },
    { "lang",         required_argument, NULL, OPTION_LANG },
    { "psm",          required_argument, NULL, OPTION_PSM },
    { "oem",          required_argument, NULL, OPTION_OEM },
    { "layout-level", required_argument, NULL, OPTION_LAYOUTLEVEL },
    { "text-levels",  required_argument, NULL, OPTION_TEXTLEVELS },
    { "only-layout",  no_argument,       NULL, OPTION_ONLYLAYOUT },
    { "xpath",        required_argument, NULL, OPTION_XPATH },
    { 0, 0, 0, 0 }
  };

/*** Functions ****************************************************************/
#define strbool( cond ) ( ( cond ) ? "true" : "false" )

void print_usage() {
  fprintf( stderr, "Description: Layout analysis and OCR using tesseract in Page XML format\n" );
  fprintf( stderr, "Usage: %s [OPTIONS] (IMAGE|PAGEXML) [OUTPUT]\n", tool );
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
  fprintf( stderr, " --xpath XPATH           xpath for selecting elements to process (def.=%s)\n", gb_xpath );
  fprintf( stderr, " -h, --help              Print this usage information and exit\n" );
  fprintf( stderr, " -v, --version           Print version and exit\n" );
  fprintf( stderr, "\n" );
  int r = system( "tesseract --help-psm 2>&1 | sed '/^ *[012] /d; s|, but no OSD||; s| (Default)||;' 1>&2" );
  if( r != 0 )
    fprintf( stderr, "warning: tesseract command not found in path\n" );
#if TESSERACT_VERSION >= 0x040000
  fprintf( stderr, "\n" );
  r += system( "tesseract --help-oem" );
#endif
}


void setCoords( tesseract::ResultIterator* iter, tesseract::PageIteratorLevel iter_level, PageXML& page, xmlNodePtr& xelem, int x = 0, int y = 0 ) {
  int left, top, right, bottom;
  iter->BoundingBox( iter_level, &left, &top, &right, &bottom );
  std::vector<cv::Point2f> points = {
    cv::Point2f(x+left,y+top),
    cv::Point2f(x+right,y+top),
    cv::Point2f(x+right,y+bottom),
    cv::Point2f(x+left,y+bottom) };
  page.setCoords( xelem, points );
}

void setLineCoords( tesseract::ResultIterator* iter, tesseract::PageIteratorLevel iter_level, PageXML& page, xmlNodePtr& xelem, int x = 0, int y = 0 ) {
  // @todo guaranty that baseline+cords form a polystripe
  setCoords( iter, iter_level, page, xelem, x, y );
  int x1, y1, x2, y2;
  iter->Baseline( iter_level, &x1, &y1, &x2, &y2 );
  std::vector<cv::Point2f> points = {
    cv::Point2f(x+x1,y+y1),
    cv::Point2f(x+x2,y+y2) };
  page.setBaseline( xelem, points );
}

void setTextEquiv( tesseract::ResultIterator* iter, tesseract::PageIteratorLevel iter_level, PageXML& page, xmlNodePtr& xelem, bool trim = false ) {
  double conf = 0.01*iter->Confidence( iter_level );
  char* text = iter->GetUTF8Text( iter_level );
  if( ! trim )
    page.setTextEquiv( xelem, text, &conf );
  else {
    std::string stext(text);
    stext = std::regex_replace( stext, std::regex("^\\s+|\\s+$"), "$1" );
    page.setTextEquiv( xelem, stext.c_str(), &conf );
  }
  delete[] text;
}


/*** Program ******************************************************************/
int main( int argc, char *argv[] ) {

  /// Disable debugging and informational messages from Leptonica. ///
  setMsgSeverity(L_SEVERITY_ERROR);

  /// Parse input arguments ///
  int n,m;
  std::stringstream test;
  std::string token;
  while( ( n = getopt_long(argc,argv,gb_short_options,gb_long_options,&m) ) != -1 )
    switch( n ) {
      case OPTION_TESSDATA:
        gb_tessdata = optarg;
        break;
      case OPTION_LANG:
        gb_lang = optarg;
        break;
      case OPTION_PSM:
        gb_psm = atoi(optarg);
        if( gb_psm < tesseract::PSM_AUTO || gb_psm >= tesseract::PSM_COUNT ) {
          fprintf( stderr, "%s: error: invalid page segmentation mode: %s\n", tool, optarg );
          return 1;
        }
        break;
      case OPTION_OEM:
        gb_oem = atoi(optarg);
        break;
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
      case OPTION_XPATH:
        gb_xpath = optarg;
        break;
      case OPTION_HELP:
        print_usage();
        return 0;
      case OPTION_VERSION:
        fprintf( stderr, "%s %.10s\n", tool, version+10 );
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

  /// Check that there is at least one and at most two non-option arguments /// 
  if ( optind >= argc || argc - optind > 2 ) {
    fprintf( stderr, "%s: error: incorrect input arguments, see usage with --help\n", tool );
    return 1;
  }

  /// Initialize tesseract just for layout or with given language and tessdata path///
  tesseract::TessBaseAPI *tessApi = new tesseract::TessBaseAPI();

  if( gb_onlylayout )
    tessApi->InitForAnalysePage();
  else
#if TESSERACT_VERSION >= 0x040000
  if( tessApi->Init( gb_tessdata, gb_lang, (tesseract::OcrEngineMode)gb_oem ) ) {
#else
  if( tessApi->Init( gb_tessdata, gb_lang) ) {
#endif
    fprintf( stderr, "%s: error: could not initialize tesseract\n", tool );
    return 1;
  }

  tessApi->SetPageSegMode( (tesseract::PageSegMode)gb_psm );

  char *ifn = argv[optind++];
  //Pix *image = NULL;
  PageXML page;
  std::vector<NamedImage> images;
  tesseract::ResultIterator* iter = NULL;

  std::regex reXml(".+\\.xml$",std::regex_constants::icase);
  std::cmatch base_match;
  bool input_xml = std::regex_match(ifn,base_match,reXml);

  /// Input is xml ///
  if ( input_xml ) {
    page.loadXml( ifn );
    images = page.crop( (std::string(gb_xpath)+"/_:Coords").c_str() );
  }

  /// Input is image ///
  else {
    /// Read input image ///
    NamedImage namedimage;
    namedimage.image = pixRead( ifn );
    if ( namedimage.image == NULL ) {
      fprintf( stderr, "%s: error: problems reading image: %s\n", tool, ifn );
      return 1;
    }
    images.push_back( namedimage );

    /// Initialize Page XML ///
    char creator[128];
    if ( gb_onlylayout )
      snprintf( creator, sizeof creator, "%s_v%.10s tesseract_v%s", tool, version+10, tesseract::TessBaseAPI::Version() );
    else
      snprintf( creator, sizeof creator, "%s_v%.10s tesseract_v%s lang=%s", tool, version+10, tesseract::TessBaseAPI::Version(), gb_lang );
    page.newXml( creator, ifn, pixGetWidth(namedimage.image), pixGetHeight(namedimage.image) );
  }

  /// Loop through all images to process ///
  for ( n=0; n<(int)images.size(); n++ ) {
    tessApi->SetImage( images[n].image );

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
    if ( gb_onlylayout )
      iter = (tesseract::ResultIterator*)( tessApi->AnalyseLayout() );

    /// Perform recognition ///
    else {
      tessApi->Recognize( 0 );
      iter = tessApi->GetIterator();
    }

    /// Loop through blocks ///
    int block = 0;
    if ( iter != NULL && ! iter->Empty( tesseract::RIL_BLOCK ) )
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
        if ( iter->BlockType() > PT_CAPTION_TEXT ) {
          if ( ! iter->Next( tesseract::RIL_BLOCK ) )
            break;
          continue;
        }

        block++;

        xmlNodePtr xreg = NULL;
        std::string rid = "b" + std::to_string(block);

        /// If xml input and region selected, set xreg to node ///
        if ( node_level == LEVEL_REGION ) {
          rid = std::string(images[n].id);
          xreg = node;
        }

        /// Otherwise add block as TextRegion element ///
        else if ( node_level < LEVEL_REGION ) {
          xreg = page.addTextRegion( "//_:Page", rid.c_str() );

          /// Set block bounding box and text ///
          setCoords( iter, tesseract::RIL_BLOCK, page, xreg, images[n].x, images[n].y );
          if ( ! gb_onlylayout && gb_textlevels[LEVEL_REGION] )
            setTextEquiv( iter, tesseract::RIL_BLOCK, page, xreg, true );
        }

        /// Set rotation and reading direction ///
        /*tesseract::Orientation orientation;
        tesseract::WritingDirection writing_direction;
        tesseract::TextlineOrder textline_order;
        float deskew_angle;
        iter->Orientation( &orientation, &writing_direction, &textline_order, &deskew_angle );
        PAGEXML_READ_DIRECTION direct = PAGEXML_READ_DIRECTION_LTR;
        float orient = 0.0;
        switch( writing_direction ) {
          case tesseract::WRITING_DIRECTION_LEFT_TO_RIGHT: direct = PAGEXML_READ_DIRECTION_LTR; break;
          case tesseract::WRITING_DIRECTION_RIGHT_TO_LEFT: direct = PAGEXML_READ_DIRECTION_RTL; break;
          case tesseract::WRITING_DIRECTION_TOP_TO_BOTTOM: direct = PAGEXML_READ_DIRECTION_TTB; break;
        }
        switch( orientation ) {
          case tesseract::ORIENTATION_PAGE_UP:    orient = 0.0;   break;
          case tesseract::ORIENTATION_PAGE_RIGHT: orient = -90.0; break;
          case tesseract::ORIENTATION_PAGE_LEFT:  orient = 90.0;  break;
          case tesseract::ORIENTATION_PAGE_DOWN:  orient = 180.0; break;
        }
        page.setRotation( xreg, orient );
        page.setReadingDirection( xreg, direct );*/

        /// Loop through paragraphs in current block ///
        int para = 0;
        while ( gb_layoutlevel >= LEVEL_REGION ) {
          para++;

          /// Loop through lines in current paragraph ///
          int line = 0;
          while ( gb_layoutlevel >= LEVEL_LINE ) {
            line++;

            xmlNodePtr xline = NULL;
            std::string lid;

            /// If xml input and line selected, set xline to node ///
            if ( node_level == LEVEL_LINE )
              xline = node;

            /// Otherwise add TextLine element ///
            else if ( node_level < LEVEL_LINE ) {
              lid = "b" + std::to_string(block) + "_p" + std::to_string(para) + "_l" + std::to_string(line);
              if ( node_level == LEVEL_REGION )
                lid = rid + "_" + lid;

              xline = page.addTextLine( xreg, lid.c_str() );
            }

            /// Set line bounding box, baseline and text ///
            if ( xline != NULL ) {
              setLineCoords( iter, tesseract::RIL_TEXTLINE, page, xline, images[n].x, images[n].y );
              if ( ! gb_onlylayout && gb_textlevels[LEVEL_LINE] )
                setTextEquiv( iter, tesseract::RIL_TEXTLINE, page, xline, true );
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
                setCoords( iter, tesseract::RIL_WORD, page, xword, images[n].x, images[n].y );
                if ( ! gb_onlylayout && gb_textlevels[LEVEL_WORD] )
                  setTextEquiv( iter, tesseract::RIL_WORD, page, xword );
              }

              /// Loop through symbols in current word ///
              while ( gb_layoutlevel >= LEVEL_GLYPH ) {
                /// Set xglyph to node or add new Glyph element depending on the case ///
                xmlNodePtr xglyph = node_level == LEVEL_GLYPH ? node : page.addGlyph( xword );

                /// Set symbol bounding box and text ///
                setCoords( iter, tesseract::RIL_SYMBOL, page, xglyph, images[n].x, images[n].y );
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

  } // for ( n=0; n<(int)images.size(); n++ ) {

  /// Write resulting XML ///
  page.setLastChange();
  page.write( optind < argc ? argv[optind] : "-" );

  /// Release resources ///
  for( n=0; n<(int)images.size(); n++ )
    pixDestroy(&(images[n].image));
  tessApi->End();
  delete tessApi;
  delete iter;

  return 0;
}
