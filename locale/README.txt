AmiHASS translations

amihass.cd   the English text and string IDs (do not renumber)
<lang>.ct    one translation per language, UTF-8, "## language" naming the
             Workbench language (svenska, deutsch, ...)

Run tools/make_catalogs.py to rebuild include/ah_strings.h and
Catalogs/<language>/amihass.catalog. Keep every %d, %ld and %s of the
English text, in the same order; the tool refuses anything else, and the
programs ignore such a translation at run time too.
