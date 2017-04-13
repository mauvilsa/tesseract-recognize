/**
 * Tool that does OCR recognition using tesseract and outputs the result in Page XML
 *
 * @version $Version: 2017.03.14$
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
static char version[] = "$Version: 2017.03.14$";

char gb_default_lang[] = "eng";
char gb_default_xpath[] = "//_:TextRegion/_:Coords";

char *gb_lang = gb_default_lang;
char *gb_tessdata = NULL;
int gb_psm = tesseract::PSM_AUTO;
int gb_oem = tesseract::OEM_DEFAULT;
bool gb_onlylayout = false;
char *gb_xpath = gb_default_xpath;
bool gb_textlevels[] = { false, false, false, false };
bool gb_textatlayout = true;

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
  OPTION_LANG        = 'l',
  OPTION_LAYOUTLEVEL = 'L',
  OPTION_XPATH       = 'x',
  OPTION_TESSDATA    = 256,
  OPTION_TEXTLEVELS       ,
  OPTION_ONLYLAYOUT       ,
  OPTION_PSM              ,
  OPTION_OEM
};

static char gb_short_options[] = "hvl:L:F:BP";

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
  fprintf( stderr, "Description: Layout analysis and text recognition using tesseract\n" );
  fprintf( stderr, "Usage: %s [OPTIONS] (IMAGE|PAGEXML) [OUTPUT]\n", tool );
  fprintf( stderr, "Options:\n" );
  fprintf( stderr, " -l, --lang LANG      Language used for OCR (def.=%s)\n", gb_lang );
  fprintf( stderr, "     --tessdata PATH  Location of tessdata (def.=%s)\n", gb_tessdata );
  fprintf( stderr, "     --psm MODE       Page segmentation mode (def.=%d)\n", gb_psm );
#if TESSERACT_VERSION >= 0x040000
  fprintf( stderr, "     --oem MODE       OCR engine mode (def.=%d)\n", gb_oem );
#endif
  fprintf( stderr, " -L, --layout-level LEVEL    Layout output level: region, line, word, glyph (def.=%s)\n", levelStrings[gb_layoutlevel] );
  fprintf( stderr, "     --text-levels L1[,L2]+  Text output level(s): region, line, word, glyph (def.=layout-level)\n" );
  fprintf( stderr, "     --only-layout    Only perform layout analysis, no text recognition (def.=%s)\n", strbool(gb_onlylayout) );
  fprintf( stderr, " -x, --xpath XPATH    xpath for selecting elements to process (def.=%s)\n", gb_xpath );
  fprintf( stderr, " -h, --help           Print this usage information and exit\n" );
  fprintf( stderr, " -v, --version        Print version and exit\n" );
  fprintf( stderr, "\n" );
  int r = system( "tesseract --help-psm 2>&1 | sed '/^ *[012] /d; s|, but no OSD||; s| (Default)||;' 1>&2" );
  if( r != 0 )
    fprintf( stderr, "error: tesseract command not in path?\n" );
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
        if( gb_psm < 3 /*|| gb_psm > 10*/ ) {
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

  if ( gb_textatlayout )
    gb_textlevels[gb_layoutlevel] = true;

  /// Check that there is at least one and at most two non-option arguments /// 
  if ( optind >= argc || argc - optind > 2 ) {
    fprintf( stderr, "%s: error: incorrect input arguments, see usage with --help\n", tool );
    return 1;
  }

  char *ifn = argv[optind++];
  Pix *image = NULL;
  PageXML page;
  std::regex reXml(".+\\.xml$",std::regex_constants::icase);
  std::cmatch base_match;
  bool input_xml = std::regex_match(ifn,base_match,reXml);

  /// Read input xml ///
  if ( input_xml )
    page.loadXml( ifn );

  /// Read input image ///
  else {
    image = pixRead( ifn );
    if ( image == NULL ) {
      fprintf( stderr, "%s: error: problems reading image\n", tool );
      return 1;
    }
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
  tesseract::ResultIterator* iter = NULL;

  if ( input_xml ) {
    std::vector<NamedImage> images = page.crop( gb_xpath );

    for( int n=(int)images.size()-1; n>=0; n-- ) {
      tessApi->SetImage( images[n].image );
      xmlNodePtr node = images[n].node->parent;

      int node_level = -1;
      if ( page.nodeIs( node, "TextRegion" ) )
        node_level = LEVEL_REGION;
      else if ( page.nodeIs( node, "TextLine" ) )
        node_level = LEVEL_LINE;
      else if ( page.nodeIs( node, "Word" ) )
        node_level = LEVEL_WORD;
      else if ( page.nodeIs( node, "Glyph" ) )
        node_level = LEVEL_GLYPH;

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
          PolyBlockType btype = iter->BlockType();
          if ( btype > PT_CAPTION_TEXT ) {
            if ( ! iter->Next( tesseract::RIL_BLOCK ) )
              break;
            continue;
          }

          block++;

          /// Add block as TextRegion element ///
          //char rid[24];
          //sprintf( rid, "b%d", block );
          //xmlNodePtr xreg = page.addTextRegion( "//_:Page", rid );
          xmlNodePtr xreg = NULL;
          std::string rid;
          if ( node_level == LEVEL_REGION ) {
            xreg = node;
            rid = std::string(images[n].id);
          }

          /// Set block bounding box and text ///
          //setCoords( iter, tesseract::RIL_BLOCK, page, xreg, images[n].x, images[n].y );
          //if ( ! gb_onlylayout && gb_textlevels[LEVEL_REGION] )
          //  setTextEquiv( iter, tesseract::RIL_BLOCK, page, xreg, true );
          // @todo Given region could be split into different blocks, so need to join recognized text into a single TextEquiv element, or split the region into multiple regions? not!

          /// Loop through paragraphs in current block ///
          int para = 0;
          while ( gb_layoutlevel >= LEVEL_REGION ) {
            para++;

            /// Loop through lines in current paragraph ///
            int line = 0;
            while ( gb_layoutlevel >= LEVEL_LINE ) {
              line++;

              //char lid[32];
              //sprintf( lid, "b%d_p%d_l%d", block, para, line );
              //xmlNodePtr xline = page.addTextLine( xreg, lid );

              xmlNodePtr xline = NULL;
              std::string lid;

              /// Set xline to the current selected node
              if ( node_level == LEVEL_LINE )
                xline = node;
                // @todo but what if given line is split into multiple lines?

              /// Add TextLine element ///
              else {
                lid = rid + "_b" + std::to_string(block) + "_p" + std::to_string(para) + "_l" + std::to_string(line);
                xline = page.addTextLine( xreg, lid.c_str() );
              }

              /// Set line bounding box, baseline and text ///
              setLineCoords( iter, tesseract::RIL_TEXTLINE, page, xline, images[n].x, images[n].y );
              if ( ! gb_onlylayout && gb_textlevels[LEVEL_LINE] )
                setTextEquiv( iter, tesseract::RIL_TEXTLINE, page, xline, true );

              /// Loop through words in current text line ///
              while ( gb_layoutlevel >= LEVEL_WORD ) {
                xmlNodePtr xword = NULL;

                /// Set xword to the current selected node
                if ( node_level == LEVEL_WORD )
                  xword = node;
                  // @todo but what if given word is split into multiple words?

                /// Add Word element ///
                else
                  xword = page.addWord( xline );

                /// Set word bounding box and text ///
                setCoords( iter, tesseract::RIL_WORD, page, xword, images[n].x, images[n].y );
                if ( ! gb_onlylayout && gb_textlevels[LEVEL_WORD] )
                  setTextEquiv( iter, tesseract::RIL_WORD, page, xword );

                /// Loop through symbols in current word ///
                while ( gb_layoutlevel >= LEVEL_GLYPH ) {
                  xmlNodePtr xglyph = NULL;

                  /// Set xglyph to the current selected node
                  if ( node_level == LEVEL_GLYPH )
                    xglyph = node;
                    // @todo but what if given glyph is split into multiple glyphs?

                  /// Add Glyph element ///
                  else
                    xglyph = page.addGlyph( xword );

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

      pixDestroy(&(images[n].image));
    } // for( int n=(int)images.size()-1; n>=0; n-- ) {
  } // if ( input_xml ) {
  else {

  tessApi->SetImage( image );

  /// Perform layout analysis ///
  if ( gb_onlylayout )
    iter = (tesseract::ResultIterator*)( tessApi->AnalyseLayout() );

  /// Perform recognition ///
  else {
    tessApi->Recognize( 0 );
    iter = tessApi->GetIterator();
  }

  /// Create Page XML object with results ///
  char creator[128];
  if ( gb_onlylayout )
    snprintf( creator, sizeof creator, "%s_v%.10s tesseract_v%s", tool, version+10, tesseract::TessBaseAPI::Version() );
  else
    snprintf( creator, sizeof creator, "%s_v%.10s tesseract_v%s lang=%s", tool, version+10, tesseract::TessBaseAPI::Version(), gb_lang );
  page.newXml( creator, ifn, pixGetWidth(image), pixGetHeight(image) );

  /// Loop through blocks ///
  int block = 0;
  if ( iter != NULL && ! iter->Empty( tesseract::RIL_BLOCK ) )
  while ( gb_layoutlevel >= LEVEL_REGION ) {
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
    PolyBlockType btype = iter->BlockType();
    if ( btype > PT_CAPTION_TEXT ) {
      if ( ! iter->Next( tesseract::RIL_BLOCK ) )
        break;
      continue;
    }

    block++;

    /// Add block as TextRegion element ///
    char rid[24];
    sprintf( rid, "b%d", block );
    xmlNodePtr xreg = page.addTextRegion( "//_:Page", rid );

    /// Set block bounding box and text ///
    setCoords( iter, tesseract::RIL_BLOCK, page, xreg );
    if ( ! gb_onlylayout && gb_textlevels[LEVEL_REGION] )
      setTextEquiv( iter, tesseract::RIL_BLOCK, page, xreg, true );

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

        /// Add TextLine element ///
        char lid[32];
        sprintf( lid, "b%d_p%d_l%d", block, para, line );
        xmlNodePtr xline = page.addTextLine( xreg, lid );

        /// Set line bounding box, baseline and text ///
        setLineCoords( iter, tesseract::RIL_TEXTLINE, page, xline );
        if ( ! gb_onlylayout && gb_textlevels[LEVEL_LINE] )
          setTextEquiv( iter, tesseract::RIL_TEXTLINE, page, xline, true );

        /// Loop through words in current text line ///
        while ( gb_layoutlevel >= LEVEL_WORD ) {
          xmlNodePtr xword = page.addWord( xline );

          /// Set word bounding box and text ///
          setCoords( iter, tesseract::RIL_WORD, page, xword );
          if ( ! gb_onlylayout && gb_textlevels[LEVEL_WORD] )
            setTextEquiv( iter, tesseract::RIL_WORD, page, xword );

          /// Loop through symbols in current word ///
          while ( gb_layoutlevel >= LEVEL_GLYPH ) {
            xmlNodePtr xglyph = page.addGlyph( xword );

            /// Set symbol bounding box and text ///
            setCoords( iter, tesseract::RIL_SYMBOL, page, xglyph );
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

  }

  /// Write resulting XML ///
  page.setLastChange();
  page.write( optind < argc ? argv[optind] : "-" );

  /// Release resources ///
  if ( image != NULL )
    pixDestroy(&image);
  tessApi->End();
  delete tessApi;
  delete iter;

  return 0;
}
