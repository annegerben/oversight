

This applies to dedicated NAS hardware as well as simple Windows shares on PCs, etc.

Oversight has a 'nas\_timeout' setting ( see menu Oversight->Setup->General Settings )

This is the amount of time oversight waits for a response from the sharing service running on any NAS for which Oversight has previously scanned media.

Oversight has this feature because it shows all media from multiple sources in the same view, and it runs independently of the NAS. Having this timeout prevents Oversight waiting for ages to display your media, before realizing your NAS is switched off for example.

Everytime Oversight displays a page, it first waits, for 'nas\_timeout' milliseconds, for each required NAS to report in.

The results of this activity can be seen on the setup page:

If the NAS responds within this time, then a green tick is displayed on the setup page.
If the NAS does not respond, then a red cross is shown.

If your NAS has to spin up for example you may get a red X the first time and a green tick if you try again immediately.

Increase the NAS timeout enough and you should always get a green tick (assuming NAS is available). however If you ever use Oversight with the NAS switched off (ie internal HD only), then every page load will be delayed by this amount of time. This time should be kept as short as reasonably possible for your network/NAS configuration.

# Recommendations #

So, if you always need your NAS on when using oversight, then set this to the amount of time your NAS takes to respond after it has been left idle for sometime. (eg 1000 ms).

If you often use Oversight with the NAS switched off, then set this to a shorter time (eg
100ms).

If you have more than one NAS, then at present each one is probed in sequence, so bear that in mind when setting the timeout.

## Firewalls ##

If you have a software firewall between the NMT and the shared drive, it may block Oversights initial probe for the presence of the drive. In this case modify your firewall rules accordingly.

The probes use the same ports as SAMBA or NFS but in the case of NFS they are TCP based.

## Apple Airport ##

There are reports of some Apple Airport's becoming unresponsive after being probed by Oversight. This is surprising because a probe is just a socket connect/disconnect. Anyway if this is the case, you can disable probes by setting the timeout to 0.

# How It Works #

If the NAS share is window/SAMBA/Cifs share, then it is the max amount of time to establish a socket connection to Port 445.

If it is an NFS share, it is the amount of time to connect to the port mapper service (TCP 111) on the device.