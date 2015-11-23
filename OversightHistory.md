Oversight grew out a requirement to easily find new stuff that had downloaded to my NMT.
It didn't really start off with the intention of being a graphical jukebox.

The NMT platform has a built in usenet client (nzbget) for which I wrote a post-processing script to automatically move and rename things in a tidy way. The problem was, because the NMT GUI does not have a 'search by date/age' feature, it was difficult to work out what had downloaded recently. Especially as, at the time, nzbget did not have a history of jobs that could be easily inspected.

First I started trying to create hard links to new media, so the new content would appear in the standard NMT file browser, with names that would show me the most recent downloads first.

This was rather yucky. It was difficult to maintain the list if media was deleted by the user, and was generally confusing.

Then I thought, rather than being constrained by the NMT browser, and hard links, I could just have my own html page that is just list a table of recent downloads. Then I can use special HTML markup in the page so that clicking on the links will play the file. This started as a html table of filenames, ordered by date, then I started scraping info from IMDB and displayed this in a text page. After that, I thoght, in for a penny, and added DVD covers etc and it just evolved from there, borrowing some ideas from the NMT jukeboxes around.

However the things I think set it apart from other jukeboxes are:

1. Use of scene names - most of the stuff downloaded is not for keeps. If I want to keep something I buy the DVD. In any case I donw want the hassle of renaming stuff , just so that it gets detected. So Oversight makes extra effort to search scene names , abbreviations etc.

2. Integral File Identification and Renaming. At the time of writing most other NMT jukeboxes rely on 3rd party add ons, and enforce naming conventions that are incompatible between jukeboxes.

2. Front page shows newest content - Again - the first thing I want to see when I launch oversight is any new content that has recently been downloaded. This gives the NMT a PVR 'what's new today' feel.

3. Running on the NMT (as does http://code.google.com/p/dmj/ ) and loading dynamic pages allows a level of functionality that is tricky to implement on the static jukeboxes. Features such as Mark as watched, delete file , auto-mounting. All tricky using the PC based , static jukeboxes.

There is a downside to this, Oversight has a rich feature set but as a result it can suffer from jack of all trades syndrome:
Because development effort is spread over so many separate areas,
Its probably the ugliest jukebox by far - but that doesn't really concern me at present.

Once the back end functions stabilise I'll concentrate on the eye-candy.