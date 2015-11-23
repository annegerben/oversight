

# Oversight Database Format #

Oversight was originally an awk script, so it uses text files for its 'database'.

The files are:
  * index.db
  * plot.db
  * db/people.db
  * db/people.site.db
  * Portrait Images - db/global/_A/ovs\_personid.jpg
  * Cover Images - db/global/_J/ovs\_id.jpg
  * FanArt Images - db/global/_J/ovs\_id.jpg_

## index.db ##

index.db is the main file where media information is stored.
Each line defines a single movie or a tv episode.


The format is

```
TAB fieldid1 TAB data1 TAB fieldid2 TAB data2 TAB ....
```

This format was chosen for a number of reasons:
  1. to be easily read by awk
  1. to be easy to edit with a text editor
  1. allow later addition of new fields that might only be present for some media
  1. to avoid fixed length records - which tend to use more space.

The fieldIds should begin with an underscore, and are presently defined in the Oversight GUI and in the backend scanner (catalog.sh)
  1. GUI http://code.google.com/p/oversight/source/browse/trunk/src/dbfield.h
  1. Scanner http://code.google.com/p/oversight/source/browse/trunk/bin/catalog/db.awk#162

Most fields are plain text, but some are compressed or refer to other information in other files. In particular, datestamps, IMDB prequel,sequel info, actor, director and writer info.

### Actor/Director/Writer Info ###

These fields reference person information in db/people.db

### Datestamps ###

Date stamps (eg fields DT IT FT ) are converted to a packed binary format:

```
01234567 01234567 01234567 01234567
--yyyyyy yyyymmmm dddddhhh hhmmmmmm
```
and then printed as hex. should have used base 128 with hindsight)

### Prequel/Sequel Info ###

prequel and sequel info is a comma separated list of imdb ids.
The ids bay be in their normal text for preceeded by 'tt' OR
they will be the id number in Base 128, where the 0 character = asci 128.
See [utils.awk basen](http://code.google.com/p/oversight/source/browse/trunk/bin/catalog/util.awk?spec=svn2010&r=1995#189)

### Genres ###

The genre field ( _G ) is a list of letter codes separated by |
E.g. "u|s|t"_

The letter codes are in catalog.conf catalog\_genre keyword

## Example ##

A quick example from my NAS index.db

First Row (gaps are tabs)
```
	_F	/mnt/HD_b2/data/Complete/Limitless.2011.720p.nfo/Limitless.2011.720p.avi	_W	24,21	_d	19
	_r	7.3	_Y	6f	_DT	6f5ac46	_rt	105	_A	23,22,20,18	_id	7	_FT	6f5ac46
	_w	0	_C	M	_nfo	Limitless.2011.720p.nfo	_R	GB:15	_l	0	_IT	6f5adc0
	_G	u|s|t	_T	Limitless	_U	 imdb:tt1219289 themoviedb:51876	
```

Which is parsed as:
```
_F	/mnt/HD_b2/data/Complete/Limitless.2011.720p.nfo/Limitless.2011.720p.avi
_W	24,21
_d	19
_r	7.3
_Y	6f
_DT	6f5ac46
_rt	105
_A	23,22,20,18
_id	7
_FT	6f5ac46
_w	0
_C	M
_nfo	Limitless.2011.720p.nfo
_R	GB:15
_l	0
_IT	6f5adc0
_G	u|s|t
_T	Limitless
_U	 imdb:tt1219289 themoviedb:51876
```

Then look up the field codes either :

in C(Oversight GUI) here http://code.google.com/p/oversight/source/browse/trunk/src/dbfield.h

or

> in awk (catalog scanner) [db\_set\_fields()](http://code.google.com/p/oversight/source/browse/trunk/bin/catalog/db.awk#162)




If there is no decode function it probably means the scanner doesn't need it, but the Oversight GUI will so look at function
[db\_row\_get\_field()](http://code.google.com/p/oversight/source/browse/trunk/src/dbitem.c#851)


### Code ###

The database functions in the Oversight gui are written in C but they are very heavily optimized, as the entire database is read at each page load, so probably wouldn't help with understanding as quick as looking at the awk code of the backendscanner. This just uses a simple string split at tab marks

but C code is here [dbread\_and\_parse\_row()](http://code.google.com/p/oversight/source/browse/trunk/src/dbitem.c#106)


## plot.db ##

Plot db has format
```
id lang:plot text
```
Where
  * 'id' is the media ID in the _U field of index.db.  It has Three formats.
    * imdb:1234 IS the plot for a movie or entire tv series.
    * imdb:1234@n Is the plot for season 'n' of the tv show.
    * imdb:1234@n@m Is the plot for episode 'm' season 'n' of the tv show.
  * 'lang' is the two letter country code._



## db/people.db ##

## db/people.site.db ##
