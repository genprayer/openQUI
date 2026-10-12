# Official iCUE profiles

The current local library contains **82 byte-exact `.cueprofile` downloads**
from the [Corsair gallery](https://www.corsair.com/us/en/explorer/icue/icue-profiles/).
The snapshot date, publisher pages, document URLs, exporter versions, sizes
and SHA-256 digests are recorded in [catalog.json](catalog.json).

`downloads/` contains the actual files, including
`doom-thedarkages--DOOM-TheDarkAges.cueprofile` and
`cyberpunk--LG_Cyberpunk_IRI5aDl.cueprofile`. Use the file paths in the catalog rather
than guessing capitalization. No profile is automatically written to hardware
by this download tool.

Refresh or reproduce the library from the repository root:

```sh
python3 tools/profiles/download_official.py
```

The maintenance script requires Python 3 and curl. It reads publisher links
without executing website JavaScript, checks each Cereal document and writes
files atomically. The previous catalog remains intact if a page fails. Existing
downloads are not removed. Requests, document sizes, gallery size and total
download bytes are bounded.

The local snapshot occupies about **369 MiB**. Downloads and failure reports
are excluded from Git; the reproducible downloader and catalog are included.
This keeps the source repository small while retaining all downloaded files
on this workstation. Publisher-created lighting artwork and profiles are not
relicensed under OpenQUI's MIT license.

Open the profile sidebar, use the import control and select a downloaded file.
See [profile behavior and conversion limits](../docs/profiles.md). A profile
download, successful parsing and verified physical playback are separate steps.
