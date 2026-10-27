# Third-party notices

The module's MIT license applies to its own code. Dependencies retain their original
notices, including notices in their source headers. The installed `licenses/` directory
contains the fetched packages' license texts and bundled codec license/copyright files.

| Dependency | Source/version | Use |
| --- | --- | --- |
| MetaHook SDK / Valve HLSDK | [MetaHookSv/MetaHook](https://github.com/MetaHookSv/MetaHook/tree/4d23b6fecd79dc949aabc2e145480cd1328d4a35) | Interface factory source and GoldSrc model structures. SDK headers carrying the original Valve notices are installed under `licenses/HLSDK`. |
| FreeImage and its bundled codecs | [hzqst/FreeImage](https://github.com/hzqst/FreeImage/tree/c68700b9fe699dbbf99f88a611065f101cba1a41) | Shared image decoder. Root FreeImage Public License and GPL texts, and codec notices, are installed under `licenses/FreeImage`. |
| ScopeExit | [SergiusTheBest/ScopeExit](https://github.com/SergiusTheBest/ScopeExit/tree/bd345da594a4675d04de663d93d00cb81b6678b2) | Resource cleanup helpers, MIT, copyright 2019 Sergey Podobry. |

Source and vendor build files are consumed without modification. Local overrides may
provide different revisions; distributors should preserve the notices from those inputs.
