# empf-thumbs

Windows Explorer thumbnail + preview handler for eufyMake `.empf` files.

## Goals

- Show embedded project preview images in Explorer
- Keep Studio as the default open handler
- Stay a small native x64 COM DLL

## Do not

- Change `.empf` file-type ownership away from `eufy.Studio.1`
- Push to `homelab` unless the owner asks
- Bulk-ingest docs from the PC; after a doc push, ingest on cursor-linux
- Touch off-limits homelab paths

## Layout

- `src/` COM handlers, EMPF reader, WIC bitmap helper
- `vendor/miniz/` ZIP inflate
- `scripts/register.ps1` / `unregister.ps1`

## Git

| Remote | Role |
|--------|------|
| `homelab` | **Canonical** → `cursor-linux:/srv/git/empf-thumbs.git` |
| `origin` | Public OSS → https://github.com/Osterman-Designs/eufymake-studio-windows-thumbnails |

Do not treat GitHub as the source of truth. Push `homelab` first when the owner asks.
