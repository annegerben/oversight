

## Using Oversight ##

### How do I make Oversight start automatically ###

Try saving the following txt as index.htm in the root folder of your HDD. For SD TV save as index\_sd.htm

```
<html>
<head>
<meta http-equiv="refresh" content="10;http://localhost:8883/oversight/oversight.cgi?">
</head>
</html>
```

The delay is to allow for the NMT processes to settle down after a cold boot.

### How can I get oversight to rename and move Films that were already on the PCH _before_ I installed oversight?**###**

A with telnet you can do

`/share/Apps/oversight/catalog.sh RENAME  /path/to/media`

E.g.

`/share/Apps/oversight/catalog.sh RENAME  /share/Completed`

This is only recommended if your are sure Oversight is getting all of the movie names correct, otherwise they will end up in a wrongly named folder. This is the reason Oversight doesn't change the name of the actual movie file by default.

TV show scanning is a little bit more consistent, but pay attention to remakes.

### How do I clear the database? ###

Delete the file /share/Apps/oversight/index.db. You can do this via the TV file browser or via Windows share etc.

Usually it is just enough to delete index.db.

To completely clear the database also delete

/share/Apps/oversight/index.db
/share/Apps/oversight/plot.db
/share/Apps/oversight/db/actors.db

To clear images

delete /share/Apps/oversight/db/global/ (all fanart, covers and portraits are in here)

To clear cached web movie pages:

delete /share/Apps/oversight/cache


### Why two types of delete? ###

You can ether :
1. Completely delete a movie from Oversight and from your Hard Drive/Storage. OR
2. Just remove a file from the Oversight database, but keep it on your hard drive. (eg Home Video or Adult Content etc.). In Oversight, this is called 'delisting'.

Some people prefer the **DELETE** button on the remote, to just Delist or hide items , others want it to really Delete  the item.

By default **DELETE** button on the remote will delist, but you can change it to a real file deletion, in the OVersight->Setup->Keys menu.

On the left is the Oversight action , on the right is the sequence of keys on the Remote to perform that action. These are also known as [Tvid codes](http://www.networkedmediatank.com/wiki/index.php/Technical_Documentation#TVID_codes)

### How do I download to an external hard drive? ###

A. To download content to an external HDD you must change the film\_folder\_fmt and tv\_file\_fmt settings in oversight->admin->renaming.

For content other than movies or tv shows, also change the oversight->admin->Nzbget unpacking->completed\_dir to be the full path to a folder on the external HDD.
See [NZBGetIntegrationAndRenaming](NZBGetIntegrationAndRenaming.md)



### When displaying by Age, how is 'Age' calculated? ###

One of Oversight's goals is to act like a PVR, so the 'age' is generally the time the item was added to Oversight. This works well for incremental scans. However when doing a full scan, age is taken from the age of the file itself. Broadcast or Release age is not used at present.

### Why does Oversight use 3 different search engines? ###

A. No one really ask this much, but it might cross their mind when using the site checker. To reduce false positives, some queries are performed against two different search engines. If they disagree, then a third engine is used for the casting vote. This has made searches much more accurate and reduced false positives. It is important that the search engines are not re-badged versions of one another or we'd get the same results.
(eg Ask = Google)

### Can I use Oversight to manage my home movies or Movies not in IMDB ###

At present, Oversight is not really ready to manage movies that do not have IMDB ids. Oversight needs the ability to edit the database, and read XMBC nfo information. Both of which are on the todo list.

In the mean time, If you have a 200 series NMT a new firmware will be out soon that has some jukebox function that may suit your needs.

### Can I improve the fanart quality ###

All html pages are 720 on NMT then up-scaled as necessary , but you can improve the quality by editing  (with a Unix friendly editor)
/share/Apps/oversight/bin/jpg\_fetch\_and\_scale line 54
[code](code.md)
  * **fa/_.hd.jpg) o="-width=1280 -height=720" ; q=85 ;;
[/code]_

change q=85 to q=90 or q=95 to see if that's better.**

### How do I select 1080p mode ###

Oversight does not have a 1080p mode, because, for HD,  the NMT file browser interface treats the full screen as 1280 wide x 720 high which corresponds to 720p.

The NMT will play video and pictures at 1080p but the file browser pages are always 720p and scaled as necessary.

### Why does site checker always fail ###

First check other internet applications. If nzbget or torrent work but Media Portal stuff fails this is probably a DNS timeout bug.
For some combinations of router DNS lookup can take 30 seconds before succeeding.
In this case I would change router DNS settings so the router does not act as a DNS server.

Some servers NMT doesnt like 64.59.184.13/14
Try some of the public dns servers in NMT Network Settings.-
not all will fix the problem

opendns - 208.67.220.220/208.67.222.222
DNS Advantage  156.154.70.1/156.154.71.1
norton DNS 198.153.192.1/198.153.194.1
Google dns 8.8.8.8/8.8.4.4

If you know what you are doing you can quickly test each in turn by using the shell, using the following steps:
  1. edit /etc/resolv.conf directly,
  1. rm /tmp/dns\_cache
  1. wget http://google.com,
repeat until success, but remember to save permanently via NMT Network Settings)

### I selected my language but plot is still in English. Why? ###

After changing locale you must force Oversight to rescan your items. To do this either:
  1. delist individual items.
  1. Select 'All media' and 'Overwrite Posters' when rescanning.

TV Shows will only show localised plots if they are present at http://thetvdb.com
> t you must first delist an item before rescanning it.

### How do I copy the Oversight, Feedtime and NZBget configuration to a new drive ###
if your drive is going to be an exact replacement and all media paths will remain the same:

- backup oversight conf: /share/Apps/oversight/conf/**.cfg files. (unpak.cfg, catalog.cfg oversight.cfg)
- [optional](optional.md)backup oversight db : Db files are index.db plot.db and entire db/global folder.
- FEEDTIME (backup folder /share/Apps/feedtime/data)
- Nzbget (backup config file /share/Apps/NZBGet/.nzbget/nzbget.conf)**

- Copy media on to new drive first.

- Install CSI, nzbget, oversight , feedtime on new drive

- restore saved nzbget.conf to /share/Apps/NZBGet/.nzbget/nzbget.conf)
- restore saved oversight .cfg files into /share/Apps/oversight/conf
- Restore oversight db files IF NOT rescan
- restore saved feedtime folder /share/Apps/feedtime/data

### If I add a location in watch\_paths do I also need to add it to scan\_paths ###

No :)

### Is there a quick way to re-install oversight if it stops working ###

Try this:

In the standard NMT file browser,
  1. go to the "Apps" folder,
  1. go to the "oversight" folder
  1. Press the blue button on the remote to see non-media files (esp If using an older nmt (eg 100 series, hdx 1000 etc)
  1. Select oversight-install.cgi
  1. Follow instructions to re-install.

## Technical FAQ ##

### Why not perl instead of awk ###

This project was started on the NMT mediaplayer platform. Perl had not been compiled. It did have php, but I wanted something that was available on most embedded platforms.
With hindsight I should have made the effort to use a language that has a good set of libraries (xml,json,http etc) , instead I had to write buggy versions of everything or launch help processes (wget,iconv) etc.
So it was a mistake, that I'm not in a hurry to fix.

### Why a text database / why not sqllite ###

Because I used awk. - Yes, on reflection awk was a bad choice!

### Why so many space optimisations that dont seem very effective ###

Oversight is a dynamic Jukebox running originally on a very low powered media player running embedded linux. 100 series(i.e. slow) building HTML pages from a database in real time, as the user loads each page.

This had to take around 200ms in total on a A100, (excluding time for NMT to render HTML to the screen which is about 0.5 secs), before usability tanked.

That is in about 2 tenths of a second to:

- reading and filtering the entire index db for entries to display, (this bit takes around 10-30ms)
- check file system for media present (eg NAS on/off or file deleted remotely),
- loading HTML templates,
- writing new HTML for the page the user just requested,

So the main thing at the time were two almost conflicting requirements:

a) to keep the index.db as small as possible because it was scanned at each page load and both NMT CPU and IO are slow. [gui, was originally an awk script before I re-wrote it in C](the.md)
b) but conversely keep it as text because scans are added by an awk script. (see FAQ on why awk!)

and also

c) allow optional and variable length fields and easy addition of new fields (hence field markers in each line)

Every character counted, hence some of the optimisations which may seen like badly re-inventing wheels, if you don't run Oversight (esp on 100 series) or are used to static jukeboxes.

I think I got the index.db load down to around 10-20ms for my small collection, and the main bottle neck for page loading was the NMT rendering the HTML page. For which there is no solution apart from Flash or doing my own rendering etc.

Going forward, now that Oversight GUI might be obsoleted in the near future/and as the rendering platforms become faster, it would make sense to relax or remove some of less optimal optimisations, in favour of clearer format.

These days most devices have plenty of RAM and CPU , and application developers don't really need to pay much attention to performance.