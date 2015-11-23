

# TV Interface #

To view the Oversight Page on the TV - select the Web Services on the NMT Menu then select "Oversight".

# Browser Interface #

To view Oversight in a browser window. Go to http://<ip address on NMT>:8883/oversight/oversight.cgi


# Setup Page #

The setup page can be accessed by clicking on the Setup icon ![http://oversight.googlecode.com/svn/trunk/images/nav/set1/configure.png](http://oversight.googlecode.com/svn/trunk/images/nav/set1/configure.png)

# Your First Scan #

Before you run your first scan you must add your media sources to **either** the scan paths **or** the watch paths.

The scan\_paths and the watch\_paths are comma separated lists of media sources..

eg /share/Video,/USB\_DRIVE\_A-1,MYNAS

The NMT runs Linux which (unlike Windows) has case sensitive filenames. This means that /share/Video and /share/video are not the same.

For a single 'one-off' scan, you should add the source to the 'scan\_paths'
If you want Oversight to regularly check the source for new content, add the source to the 'watch\_paths'.

**Do not add the source to both.** - I'll check for this soon.

By default Oversight is initially configured to scan the following folders (used by the 'nzbget' post-processing script)

  * /share/Complete
  * /share/Video
  * /share/Tv
  * /share/Movies

The **/share/Download** folder is ignored. This is because this folder may contain incomplete video. If you have complete video's in this folder, it is probably wise to move them to a different folder, (eg /share/Video ). Alternatively, if you really want all the content in /share/Download appearing then remove /share/Download from the 'ignore\_paths' setting.

## Source on Internal HDD ##
To add a folder on the Internal HDD just enter the path to the folder. E.g.
```
/share/Video
```

All paths on the internal HDD must start with '/share/'

## Source on USB ##

To add a folder on USB enter the path to the folder. This can be just the USB device name with a leading '/'. E.g.
```
/USB_DRIVE_A-1
```

## Source on a NAS ##

To add a folder on a network drive, you must first define the source in the standard NMT Network Sources setup window, and make sure you can browse to it OK.
For example the following link shows a network source called 'test' pointing to a server called TRACY-SNG and the folder on the share is called Videos.

![http://www.networkedmediatank.com/wiki/images/1/1f/Setup_08.jpg](http://www.networkedmediatank.com/wiki/images/1/1f/Setup_08.jpg)

(image linked from networkedmediatank.com wiki)

Once the share has been set up, Then add the source name (in this case 'test' ) to Oversight's scan\_paths _or_ watch\_paths, _without any leading slash._ .
```
scan_paths=test
```
OR if you want Oversight to keep watching for new videos:
```
watch_paths=test
```



This will scan the folder 'Videos'. If a specific folder on the shared drive is required then also add '/' followed by the folder name. E.g.
```
scan_paths=test/Movies
```
OR
```
watch_paths=test/Movies
```

will now scan smb://TRACY-SNG/Videos/Movies


If a location is specified in _watch\_paths_ you do not need to repeat it again in _scan\_paths_.

After you have set up the folders click the 'Rescan All Media' option ![http://oversight.googlecode.com/svn/trunk/images/nav/set1/rescan.png](http://oversight.googlecode.com/svn/trunk/images/nav/set1/rescan.png) to start a background scan.

Also for efficient nas scanning see AutomaticNASScanning

If the scan does not work at all, try replacing the NAS name with it's IP address in the Network Setup screen and in the Oversight settings. Also see [NASTimeouts](NASTimeouts.md)