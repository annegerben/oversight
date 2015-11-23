Oversight employs a number of different search methods:

# TV Searches #

TBC

# Movie Searches #

For movies Oversight uses the following searches:

## ONLINE\_NFO ##
> Searches nfo files at bintube and binsearch websites.
    * Excellent for scene release filenames.
    * OK for movies with distinctive titles in the filename.
    * Bad for movies with filenames consisting of a few common English words. eg Wanted.avi
    * Bad for remakes with smae titles.

## IMDBTITLE ##
> Extracts the main keywords from the title , removes any format info (eg DVDRip),
and then searches the web for those words and "imdb" and picks the first one.
  * Good for movies with distinctive titles in the filename.
  * Hit an Miss for Remakes unless the year is specified in the filename.
  * Poor for Scene release filenames.

## IMDBLINK ##
> Extracts the main keywords from the title , removes any format info (eg DVDRip),
and then searches the web for those words and "imdb" ,and selects a link which occurs significantly more often than other links.
  * OK for scene releases.
  * Good for movies with distinctive titles in the filename.
  * Good at not picking anything if it can be confused with a remake. (Low false positives but then needs another search method to finish the job)

## IMDB ##
> Similar to IMDBLINK but only searches the imdb.com site via a public search engine.
> This is more tolerant of unknown formatting info in the file compared to using IMDB's own search - which can throw up odd results eg..
http://www.imdb.com/find?s=all&q=transformers+2+2009
vs
http://uk.search.yahoo.com/search?p=transformers+2+2009+site%3Aimdb.com