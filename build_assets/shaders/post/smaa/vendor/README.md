# SMAA

Source: https://github.com/iryoku/smaa, revision
`71c806a838bdd7d517df19192a20f0c61b3ca29d`.

`build_assets/shaders/post/smaa/vendor/smaa.hlsli` is the upstream shader.
Trailing whitespace was removed from the vendored copy.
`build_assets/resources/smaa/area.png` and `search.png` were converted without resampling
from the upstream DDS lookup textures into RGBA PNG for the runtime texture cache.
The accompanying license applies to the upstream files and lookup data.
