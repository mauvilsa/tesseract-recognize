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
#include <../tesseract/baseapi.h>
#include <../leptonica/allheaders.h>
#include <algorithm>
#include <string>
#include <getopt.h>
#include <time.h>

//#ifdef __TESSERACT_SOURCE__
//#include "pageres.h"
//#include "ocrrow.h"
//#endif

/*** Definitions **************************************************************/
static char tool[] = "tesseract-recognize";
static char version[] = "$Version: 2017.03.14$";

#define OUT_ASCII 0
#define OUT_XMLPAGE 1

char gb_default_lang[] = "eng";

char *gb_lang = gb_default_lang;
char *gb_tessdata = NULL;
int gb_psm = tesseract::PSM_AUTO_ONLY;
#if TESSERACT_VERSION >= 0x040000
int gb_oem = tesseract::OEM_DEFAULT;
#endif
int gb_level = 3;
int gb_format = OUT_XMLPAGE;
bool gb_regblock = true;

enum {
  OPTION_HELP       = 'h',
  OPTION_VERSION    = 'v',
  OPTION_LANG       = 'l',
  OPTION_LEVEL      = 'L',
  OPTION_FORMAT     = 'F',
  OPTION_BLOCKS     = 'B',
  OPTION_PARAGRAPHS = 'P',
  OPTION_TESSDATA   = 256,
  OPTION_PSM             ,
  OPTION_OEM
};

static char gb_short_options[] = "hvl:L:F:BP";

static struct option gb_long_options[] = {
    { "help",        no_argument,       NULL, OPTION_HELP },
    { "version",     no_argument,       NULL, OPTION_VERSION },
    { "tessdata",    required_argument, NULL, OPTION_TESSDATA },
    { "lang",        required_argument, NULL, OPTION_LANG },
    { "psm",         required_argument, NULL, OPTION_PSM },
    { "oem",         required_argument, NULL, OPTION_OEM },
    { "level",       required_argument, NULL, OPTION_LEVEL },
    { "format",      required_argument, NULL, OPTION_FORMAT },
    { "blocks",      no_argument,       NULL, OPTION_BLOCKS },
    { "paragraphs",  no_argument,       NULL, OPTION_PARAGRAPHS },
    { 0, 0, 0, 0 }
  };

/*** Functions ****************************************************************/
#define strbool( cond ) ( ( cond ) ? "true" : "false" )

void print_usage() {
  fprintf( stderr, "Description: OCR recognition using tesseract\n" );
  fprintf( stderr, "Usage: %s [OPTIONS] IMAGE [OUTPUT]\n", tool );
  fprintf( stderr, "Options:\n" );
  fprintf( stderr, " -l, --lang LANG      Language used for OCR (def.=%s)\n", gb_lang );
  fprintf( stderr, "     --tessdata PATH  Location of tessdata (def.=%s)\n", gb_tessdata );
  fprintf( stderr, "     --psm MODE       Page segmentation mode (def.=%d)\n", gb_psm );
#if TESSERACT_VERSION >= 0x040000
  fprintf( stderr, "     --oem MODE       OCR engine mode (def.=%d)\n", gb_oem );
#endif
  fprintf( stderr, " -L, --level LEVEL    Layout level: 1=blocks, 2=paragraphs, 3=lines, 4=words, 5=chars (def.=%d)\n", gb_level );
  fprintf( stderr, " -F, --format FORMAT  Output format, either 'ascii' or 'xmlpage' (def.=xmlpage)\n" );
  fprintf( stderr, " -B, --blocks         Use blocks for the TextRegions (def.=%s)\n", strbool(gb_regblock) );
  fprintf( stderr, " -P, --paragraphs     Use paragraphs for the TextRegions (def.=%s)\n", strbool(!gb_regblock) );
  fprintf( stderr, " -h, --help           Print this usage information and exit\n" );
  fprintf( stderr, " -v, --version        Print version and exit\n" );
  fprintf( stderr, "\n" );
  int r = system( "tesseract --help-psm 2>&1 | sed '/^ *[012] /d; s|, but no OSD||;' 1>&2" );
#if TESSERACT_VERSION >= 0x040000
  fprintf( stderr, "\n" );
  r += system( "tesseract --help-oem" );
#endif
}

void xmlEncode(std::string& data) {
    std::string buffer;
    buffer.reserve(data.size());
    for(size_t pos = 0; pos != data.size(); ++pos) {
        switch(data[pos]) {
            case '&':  buffer.append("&amp;");       break;
            case '\"': buffer.append("&quot;");      break;
            case '\'': buffer.append("&apos;");      break;
            case '<':  buffer.append("&lt;");        break;
            case '>':  buffer.append("&gt;");        break;
            default:   buffer.append(&data[pos], 1); break;
        }
    }
    data.swap(buffer);
}

/*** Program ******************************************************************/
int main( int argc, char *argv[] ) {
  int err = 0;

  /// Disable debugging and informational messages from Leptonica. ///
  setMsgSeverity(L_SEVERITY_ERROR);

  /// Parse input arguments ///
  int n,m;
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
      case OPTION_LEVEL:
        gb_level = atoi(optarg);
        if( gb_level < 1 || gb_level > 5 ) {
          fprintf( stderr, "%s: error: invalid layout level: %s\n", tool, optarg );
          return 1;
        }
        break;
      case OPTION_FORMAT:
        if( ! strcasecmp(optarg,"ascii") )
          gb_format = OUT_ASCII;
        else if( ! strcasecmp(optarg,"xmlpage") )
          gb_format = OUT_XMLPAGE;
        else {
          fprintf( stderr, "%s: error: unknown output format: %s\n", tool, optarg );
          return 1;
        }
        break;
      case OPTION_BLOCKS:
        gb_regblock = true;
        break;
      case OPTION_PARAGRAPHS:
        gb_regblock = false;
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

  if( optind >= argc ) {
    fprintf( stderr, "%s: error: expected an image to process, see usage with --help\n", tool );
    return 1;
  }

  /// Read image ///
  char *ifn = argv[optind++];
  Pix *image = pixRead( ifn );
  if( image == NULL )
    return 1;

  /// Initialize tesseract with given language, without specifying tessdata path ///
//#ifdef __TESSERACT_SOURCE__
//  tesseract::MyTessBaseAPI *tessApi = new tesseract::MyTessBaseAPI();
//#else
  tesseract::TessBaseAPI *tessApi = new tesseract::TessBaseAPI();
//#endif
#if TESSERACT_VERSION >= 0x040000
  if( tessApi->Init( gb_tessdata, gb_lang, (tesseract::OcrEngineMode)gb_oem ) ) {
#else
  if( tessApi->Init( gb_tessdata, gb_lang) ) {
#endif
    fprintf(stderr, "Could not initialize tesseract.\n");
    exit(1);
  }
  tessApi->SetPageSegMode( (tesseract::PageSegMode)gb_psm );
  tessApi->SetImage( image );

  /// Perform recognition ///
  tessApi->Recognize( 0 );

//#ifdef __TESSERACT_SOURCE__
//  tesseract::MyResultIterator *iter = tessApi->GetIterator();
//#else
  tesseract::ResultIterator* iter = tessApi->GetIterator();
//#endif

  /// Output result in the selected format ///
  bool xmlpage = gb_format == OUT_XMLPAGE ? true : false ;

  /// Open file for writing output ///
  FILE *ofs = stdout;
  if( optind < argc &&
      strcmp("-",argv[optind]) &&
      (ofs=fopen(argv[optind],"wb")) == NULL ) {
    fprintf( stderr, "%s: error: unable to write to file %s\n", tool, argv[optind] );
    return 1;
  }

  /// Output results ///
  if ( xmlpage ) {
    char buf[80];
    time_t now = time(0);
    struct tm tstruct;
    tstruct = *localtime( &now );
    strftime( buf, sizeof(buf), "%Y-%m-%dT%X", &tstruct );

    fprintf( ofs, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" );
    fprintf( ofs, "<PcGts xmlns=\"http://schema.primaresearch.org/PAGE/gts/pagecontent/2013-07-15\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://schema.primaresearch.org/PAGE/gts/pagecontent/2013-07-15 http://schema.primaresearch.org/PAGE/gts/pagecontent/2013-07-15/pagecontent.xsd\">\n" );
    fprintf( ofs, "  <Metadata>\n" );
    fprintf( ofs, "    <Creator>%s</Creator>\n", tool );
    fprintf( ofs, "    <Created>%s</Created>\n", buf );
    fprintf( ofs, "    <LastChange>%s</LastChange>\n", buf );
    fprintf( ofs, "  </Metadata>\n" );
    fprintf( ofs, "  <Page imageFilename=\"%s\" imageWidth=\"%d\" imageHeight=\"%d\">\n", ifn, image->w, image->h );
  }

  int x1, y1, x2, y2;
  int left, top, right, bottom;
  tesseract::Orientation orientation;
  tesseract::WritingDirection writing_direction;
  tesseract::TextlineOrder textline_order;
  float deskew_angle;
  tesseract::ParagraphJustification just;
  bool is_list, is_crown;
  int indent;

  char direct[48];
  char orient[48];
  char xheight[48]; xheight[0] = '\0';

  int block = 0;
  if ( iter != NULL && ! iter->Empty( tesseract::RIL_BLOCK ) )
  while ( gb_level > 0 ) {
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

    block ++;
    iter->BoundingBox( tesseract::RIL_BLOCK, &left, &top, &right, &bottom );
    iter->Orientation( &orientation, &writing_direction, &textline_order, &deskew_angle );
    if ( xmlpage ) {
      switch( writing_direction ) {
        case tesseract::WRITING_DIRECTION_LEFT_TO_RIGHT:
          direct[0] = '\0';
          break;
        case tesseract::WRITING_DIRECTION_RIGHT_TO_LEFT:
          sprintf( direct, " readingDirection=\"right-to-left\"" );
          break;
        case tesseract::WRITING_DIRECTION_TOP_TO_BOTTOM:
          sprintf( direct, " readingDirection=\"top-to-bottom\"" );
          break;
      }
      switch( orientation ) {
        case tesseract::ORIENTATION_PAGE_UP:
          orient[0] = '\0';
          break;
        case tesseract::ORIENTATION_PAGE_RIGHT:
          sprintf( orient, " readingOrientation=\"-90\"" );
          break;
        case tesseract::ORIENTATION_PAGE_LEFT:
          sprintf( orient, " readingOrientation=\"90\"" );
          break;
        case tesseract::ORIENTATION_PAGE_DOWN:
          sprintf( orient, " readingOrientation=\"180\"" );
          break;
      }
    }

    if ( ! xmlpage )
      fprintf( ofs, "block %d : %dx%d+%d+%d\n", block, right-left, bottom-top, left, top );
    else if ( gb_regblock ) {
      fprintf( ofs, "    <TextRegion id=\"b%d\"%s%s>\n", block, direct, orient );
      fprintf( ofs, "      <Coords points=\"%d,%d %d,%d %d,%d %d,%d\"/>\n",
        left, top,   right, top,   right, bottom,   left, bottom );
    }

    int para = 0;
    while ( gb_level > 1 ) {
      para ++;
      iter->BoundingBox( tesseract::RIL_PARA, &left, &top, &right, &bottom );
      iter->ParagraphInfo( &just, &is_list, &is_crown, &indent );
      if ( ! xmlpage ) {
        fprintf( ofs, "paragraph %d :", para );
        if ( just == tesseract::JUSTIFICATION_LEFT )
          fprintf( ofs, " left" );
        else if ( just == tesseract::JUSTIFICATION_CENTER )
          fprintf( ofs, " center" );
        else if ( just == tesseract::JUSTIFICATION_RIGHT )
          fprintf( ofs, " right" );
        if ( is_list )
          fprintf( ofs, " list" );
        if ( is_crown )
          fprintf( ofs, " crown" );
        else if ( indent != 0 )
          fprintf( ofs, " %d", indent );
        fprintf( ofs, " %dx%d+%d+%d\n", right-left, bottom-top, left, top );
      }
      else if ( ! gb_regblock ) {
        fprintf( ofs, "    <TextRegion id=\"b%d_p%d\"%s%s>\n", block, para, direct, orient );
        fprintf( ofs, "      <Coords points=\"%d,%d %d,%d %d,%d %d,%d\"/>\n",
          left, top,   right, top,   right, bottom,   left, bottom );
      }

      int line = 0;
      while ( gb_level > 2 ) {
        line ++;
        iter->BoundingBox( tesseract::RIL_TEXTLINE, &left, &top, &right, &bottom );
        iter->Baseline( tesseract::RIL_TEXTLINE, &x1, &y1, &x2, &y2 );

//#ifdef __TESSERACT_SOURCE__
//        sprintf( xheight, xmlpage ? " custom=\"x-height: %gpx;\"" : " %g", iter->getXHeight() );
//#endif

        if ( ! xmlpage )
          fprintf( ofs, "line %d : %d,%d %d,%d %dx%d+%d+%d%s\n", line, x1, y1, x2, y2, right-left, bottom-top, left, top, xheight );
        else {
          fprintf( ofs, "      <TextLine id=\"b%d_p%d_l%d\"%s>\n", block, para, line, xheight );
          fprintf( ofs, "        <Coords points=\"%d,%d %d,%d %d,%d %d,%d\"/>\n",
            left, top,   right, top,   right, bottom,   left, bottom );
          fprintf( ofs, "        <Baseline points=\"%d,%d %d,%d\"/>\n", x1, y1, x2, y2 );
        }

        int word = 0;
        while ( gb_level > 3 ) {
          word ++;
          iter->BoundingBox( tesseract::RIL_WORD, &left, &top, &right, &bottom );
          const char* text = iter->GetUTF8Text( tesseract::RIL_WORD );
          std::string stext(text);
          xmlEncode(stext);
          double conf = iter->Confidence( tesseract::RIL_WORD );
          if ( ! xmlpage )
            fprintf( ofs, "word %d : %dx%d+%d+%d :: %g :: %s\n", word, right-left, bottom-top, left, top, conf, stext.c_str() );
          else {
            fprintf( ofs, "        <Word id=\"b%d_p%d_l%d_w%d\">\n", block, para, line, word );
            fprintf( ofs, "          <Coords points=\"%d,%d %d,%d %d,%d %d,%d\"/>\n",
              left, top,   right, top,   right, bottom,   left, bottom );
          }
          delete[] text;

          int glyph = 0;
          while ( gb_level > 4 ) {
            glyph ++;
            iter->BoundingBox( tesseract::RIL_SYMBOL, &left, &top, &right, &bottom );
            const char* text = iter->GetUTF8Text( tesseract::RIL_SYMBOL );
            std::string stext(text);
            xmlEncode(stext);
            if ( ! xmlpage )
              fprintf( ofs, "glyph %d : %dx%d+%d+%d\n", glyph, right-left, bottom-top, left, top );
            else {
              fprintf( ofs, "          <Glyph id=\"b%d_p%d_l%d_w%d_g%d\">\n", block, para, line, word, glyph );
              fprintf( ofs, "            <Coords points=\"%d,%d %d,%d %d,%d %d,%d\"/>\n",
                left, top,   right, top,   right, bottom,   left, bottom );
              fprintf( ofs, "            <TextEquiv><Unicode>%s</Unicode></TextEquiv>\n", stext.c_str() );
              fprintf( ofs, "          </Glyph>\n" );
            }
            delete[] text;

            if ( iter->IsAtFinalElement( tesseract::RIL_WORD, tesseract::RIL_SYMBOL ) )
              break;
            iter->Next( tesseract::RIL_SYMBOL );
          }

          if ( xmlpage ) {
            fprintf( ofs, "          <TextEquiv conf=\"%g\"><Unicode>%s</Unicode></TextEquiv>\n", 0.01*conf, stext.c_str() );
            fprintf( ofs, "        </Word>\n" );
          }

          if ( iter->IsAtFinalElement( tesseract::RIL_TEXTLINE, tesseract::RIL_WORD ) )
            break;
          iter->Next( tesseract::RIL_WORD );
        }

        if ( xmlpage ) {
          if ( gb_level == 3 ) {
            const char* text = iter->GetUTF8Text( tesseract::RIL_TEXTLINE );
            std::string stext(text);
            stext.erase(std::remove(stext.begin(), stext.end(), '\n'), stext.end());
            xmlEncode(stext);
            double conf = iter->Confidence( tesseract::RIL_TEXTLINE );
            fprintf( ofs, "        <TextEquiv conf=\"%g\"><Unicode>%s</Unicode></TextEquiv>\n", 0.01*conf, stext.c_str() );
            delete[] text;
          }
          fprintf( ofs, "      </TextLine>\n" );
        }

        if ( iter->IsAtFinalElement( tesseract::RIL_PARA, tesseract::RIL_TEXTLINE ) )
          break;
        iter->Next( tesseract::RIL_TEXTLINE );
      }

      if ( xmlpage && ! gb_regblock )
        fprintf( ofs, "    </TextRegion>\n" );

      if ( iter->IsAtFinalElement( tesseract::RIL_BLOCK, tesseract::RIL_PARA ) )
        break;
      iter->Next( tesseract::RIL_PARA );
    }

    if ( xmlpage && gb_regblock )
      fprintf( ofs, "    </TextRegion>\n" );

    if ( ! iter->Next( tesseract::RIL_BLOCK ) )
      break;
  }

  if ( xmlpage )
    fprintf( ofs, "  </Page>\n</PcGts>\n" );

  if( ofs != stdout )
    fclose(ofs);
  pixDestroy(&image);
  tessApi->End();
  delete tessApi;
  delete iter;

  return 0;
}
