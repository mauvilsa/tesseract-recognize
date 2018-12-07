ARG TESSREC_TAG
FROM mauvilsa/tesseract-recognize:TESSREC_TAG

### Install all language packages ###
RUN apt-get update --fix-missing \
 && apt-get install -y --no-install-recommends \
      tesseract-ocr-* \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*
