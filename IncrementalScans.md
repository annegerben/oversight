

# Incremental Scans #

## Incremental Scan Request from Nzbget on NMT ##

When oversight is installed it updates the nzbget configuration with its own post processing script. This performs unpacking an renaming of downloads. The script is
```
/share/Apps/oversight/bin/unpak.sh
```

See nzbget.conf post processing option 'PostProcess'. This script tries to avoid initiating a unrar  that will fail because of missing files or bad rar headers, or incorrect part lengths.

## Incremental Scan Request via a URL ##

Scans can be launched remotely via a URL.

```
wget -O - 'http://nmt_ipaddress:8883/oversight/oversight.cgi?view=admin&action=rescan_request&rescan_opt_@group1=NEWSCAN&rescan_dir_/path/to/your/media/folder_or_file=on'
```
This URL was originally intended for use by the GUI only, so there will be a lot of output that can be ignored if invoking from another program. Oversight on NMT will be getting a more structured API in the near future.

---

```
&rescan_opt_@group1=NEWSCAN
```
tells Oversight to do an incremental scan. Without this it will re-process ALL media in the specified folders.

---

```
&rescan_dir_/path/to/media=on
```
instructs Oversight to scan the NMT path '/path/to/media'

---

For mounted shares use
```
&rescan_dir_share_name/path/to/media
```
where **share\_name** is the name of the mount point defined in the NMT Network Source setup. E.g. for a folder /Movies on share 'mynas' use:
```
&rescan_dir_mynas/Movies=on
```

---

The path can also be very specific. This makes the update run faster. Any spaces or 'odd' characters in the path should be URL Encoded. E.g.
```
&rescan_dir_mynas/Movies/Gigli%202003=on
```

---

There can be multiple `&rescan_dir_/path/to/media1=on` options in the same URL.


## Incremental Scan Request when new content is added to a folder ##

In the 'setup->media sources' screen , oversight can be configured to regularly poll a folder. This will force deep directory search of the folders listed in the 'watch\_paths' setting.

To avoid impacting NMT normal function, this value should be the longest your are prepared to wait.

Ideally this option should be used with the trigger\_files option. Oversight will then only do the recursive scan if the date on any trigger files is newer than the last scan.

If possible configure your library update software to use the URL scan request below.

## Manual Rescanning ##

You can also rescan a particular item as follows:

  1. In the Oversight GUI, _delist_ the item to be rescanned (don't _delete_ it!!)
  1. Go to the Admin Screen, and select only the paths that contain your changed items, and select 'new items only'
  1. Rescan.

This method looks through all folders for files that are not in the index or are not categorized as a movie nor a tv show, so it may take 5 minutes or more.

I plan to have a rescan button against a particular item.
First we need an edit option to change IMDB Id., fanart & poster links.
Then "save" or "save and rescan"

---


For posterity, the old way of doing it (via telnet) still works, but is not needed any more:

  1. log in
  1. type 'cd /share/Apps/oversight'
  1. type './catalog.sh /path/to/new/media/folder'
  1. Or to rescan AND get new posters type './catalog.sh UPDATE\_POSTERS UPDATE\_FANART /path/to/new/media/folder'

See also [FullScans](FullScans.md)