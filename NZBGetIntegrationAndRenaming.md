

#summary NZBGet Integration and File Renaming

# NZBGet Integration and File Renaming #

This page is about how Oversight integrates with NZBget when renaming files.
Information here can help you change the download location and final resting place of your media downloaded via usenet.

For more general information on manual scanning and renaming see [FileNames](FileNames.md)



# Details #

when a file is downloaded it goes through the following stages.

  1. nzbget downloads the archive files into **/share/Download**
  1. Once all parts are downloaded , the archive is par-repaired and unpacked by unpak.sh into **/share/Complete** (This is an nzbget post processing script) - see [Par-repair on demand ](.md)
  1. The unpak.sh post processing script checks if the user has set a nzbget category. **If nzbget categories are used, processing stops** and the file will not appear in oversight. It's assumed the users manual category selection takes priority over oversight processing.
  1. The unpak.sh post processing script checks if the newsgroup name should be used to determine the final location of the file. This is based on the newsgroup name configuration in unpak.cfg. eg stuff from alt.binaries.sounds is moved to /share/Music, alt.binaries.erotica is moved to /share/~Other/2/4/6/8. **If there is a newsgroup name matches, the file is moved and processing stops.**
  1. Finally, if the file looks like a movie or a TV show, it is renamed by catalog.sh (This is Oversight's scanning script catalog.sh)

So if you want to change the location of these folders, refer to the relevant section below.

### The nzbget download location : **DestDir** ###

  * GUI: http://nmt_ip:9999/NZBget_web/config.php?section=S-PATHS (CSI nzbget only)
  * CLI: Change DestDir in nzbget.conf


### The unpak.sh Complete location : **completed\_dir** ###

  * GUI: Oversight -> Setup -> NZBGet Unpacking
  * CLI: vi /share/Apps/oversight/conf/unpak.cfg -> unpak\_completed\_dir

By default the completed\_dir location is where files are moved when they have finished downloading and unpacking. They will either remain here , or be moved again if

  1. The newsgroups match the newsgroup\_categories in unpak.cfg (eg if the newsgroup contains 'sounds' then it it moved to /share/Music/.
  1. catalog.sh thinks the file is a movie or tv show. (see 'TV and Movie File destinations' )

Some people set the Completed folder to be the same as the Download folder. I think to avoid having another folder on the main menu. However there are problems doing this:

By making them the same folder you then have to remove /share/Download from the ignore list,
and then if you do rescans Oversight will pick up everything in Download folder including half completed torrents etc.
In this case I recommend you change both NZBGet and Transmission to download to something like
/share/Download/InProgress, (rather than /share/Download) create the folder manually and add it to the ignore path. (see previous section for changing nzbget download folder)

### The unpak.sh custom locations (eg Music / Erotica ) ###

These options look at the newsgroup names embedded in the nzb file.

  * CLI: vi /share/Apps/oversight/conf/unpak.cfg
edit
```
unpak_auto_categorisation_from_newsgroups=1
unpak_subfolder_by_newsgroup_0="erotica:PIN:FOLDER"
unpak_subfolder_by_newsgroup_1="sounds:/share/Music"
unpak_subfolder_by_newsgroup_2="pictures:Pictures"
unpak_subfolder_by_newsgroup_3="hou:PIN:FOLDER"
```

Also for erotica review any keywords starting with unpak\_nmt\_pin

  * GUI: No GUI option

### TV and Movie File destinations ###

  * CLI: vi /share/Apps/oversight/conf/catalog.cfg
  * GUI: Oversight -> Setup -> Renaming

Eg. For movies a setting of
```
/share/Video/{:TITLE:}{ (:YEAR:)}{ - :CERT:}
```
Would move dark\_knight\_720p.mkv to
```
/share/Video/The Dark Knight (2008) - 15/dark_knight_720p.mkv
```

For movies, **only the containing folder is renamed**, never the actual file.
This is because a false match would make it very hard to later correctly identify the movie later. Even nfo files occasionally have the wrong imdb link in.

Tv file names are a bit more consistent so Oversight will rename Tv files, but not movie files.

Each bit of information has its own brackets. Anything outside the brackets is kept as fixed text, anything inside the brackets is replaced with the keyword.

So the format is:

```
{:keyword1:} fixed text {:keyword2:} more fixed text   etc.
```

A keyword always has a colon immediately before and after.

But sometimes keywords can be blank - eg if there is no episode title then

```
{:EPISODE:} - {:EPTITLE:}.{:EXT:}
```

will end up looking like

```
2 - .avi
```

In this case it would be better without the ' - ',so you can include some 'optional text' around the keyword, that is only added if the keyword is not blank... This is done by moving the fixed text inside of the brackets. so

```
{:EPISODE:}{ - :EPTITLE:}.{:EXT:}
```

Note the dash is now only added if EPTITLE is not blank.


### Par-repair on demand ###

Note that newer versions of nzbget have the capability to do on-demand par repair, however the version that comes with the NMT firmware cannot attempt to unpack before doing a par-repair on demand, it can only be configured to
a) unpack and fail,
b) force a par check before attempting to unpack.
c) hand over to a post-processing script.

As a par-check can take several hours on the NMT platform, it was desirable to try to unpack first, and only do a par-repair if necessary.

### Failing to run unpak script ###

If the unpak script is not running, then check the logs in /share/Apps/oversight/logs.

If there are no recent unpak.log files then nzbget is no running the unpak script.

Make sure Oversight has been re-installed after any upgrades to nzbget
(you can do this via the reinstall option in the Oversight admin pages).

Also , the nzbget gui restarts nzbget as the nobody user, which can cause problems. (it seems to ignore the DaemnonUser setting in the nzbget.conf file).

A temporary fix is to edit /share/Apps/NZBget/daemon.sh and change

```
    /share/Apps/NZBget/bin/nzbget -D -c /share/Apps/NZBget/.nzbget/nzbget.conf
```

to

```
    su - nmt -s /bin/sh -c "/share/Apps/NZBget/bin/nzbget -D -c /share/Apps/NZBget/.nzbget/nzbget.conf"
```

Then stop and start nzbget.