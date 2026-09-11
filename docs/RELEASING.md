# Releasing to the core catalogue

This covers what a GitHub release needs before filing a catalogue listing,
and how that listing works. Nothing here has been submitted; this is
research only.

## Which catalogue

Target **openFPGA Library** (`openfpga-library/analogue-pocket` on GitHub,
<https://github.com/openfpga-library/analogue-pocket>). This is the same
project also known as "openfpga-cores-inventory": that name was the
project's original GitHub org, `openfpga-cores-inventory`, which GitHub now
redirects to `openfpga-library` (confirmed via the GitHub API: a request for
`repos/openfpga-cores-inventory/analogue-pocket` resolves to
`full_name: "openfpga-library/analogue-pocket"`). The old org's own inventory
repo, `joshcampbell191/openfpga-cores-inventory`, is archived.

This is the right target because the updater tools read from it directly:

- Pupdate's README links its core list as "openFPGA cores inventory" at
  `https://openfpga-cores-inventory.github.io/analogue-pocket/` (the old
  Pages URL, still served).
- pocket-sync's README credits "The OpenFPGA Library" for the data it uses,
  and its FAQ says a missing core "could just be that it's not yet added to
  https://github.com/openfpga-library/analogue-pocket".

pocket-sync also lets a user drag a `.zip` onto the app to install it
directly, bypassing the catalogue, but that is a manual per-user action, not
a listing.

## What the catalogue expects, and where it goes

openFPGA Library publishes a read-only JSON API
(`https://openfpga-library.github.io/analogue-pocket/api/v3/cores.json`,
documented at `.../api/swagger/`). Its `InventoryCore` / `Release` schema
(`AnalogueCore.metadata`, `AnalogueData.data_slots`, etc.) matches the fields
already inside our own `Cores/negenii.OpenJazz/core.json` and `data.json`;
the API is built by reading the standard openFPGA core/data JSON out of the
release asset itself. **No extra metadata file needs to be authored for the
catalogue.** The only thing the maintainers need from us is the repository
URL, given through a GitHub issue:

> To add a new core, go to Issues, select the `New Issue` button, choose
> `Add Core`, then fill in the fields.
> (`openfpga-library/analogue-pocket` README)

The issue template (`.github/ISSUE_TEMPLATE/ADD-CORE.yml`) asks for exactly:

- **GitHub Repository** (required): e.g. `negenii/openJazz_pocket`.
- **Asset Filter** (optional): a regex, only needed "if the GitHub release
  contains multiple assets to narrow the selection." A release with a single
  asset "needs no further options."
- **Path** (optional): used only if the core is *not* distributed via a
  GitHub release but as a raw file checked into a repository (example given:
  `pocket/zips/jotego.jtcps1.zip`).

There is no separate form, YAML file, or PR into the inventory repo beyond
that one issue.

One thing outside our repo that's genuinely uncertain: the API also exposes
a `Platform` entity (id, category, name, manufacturer, year) alongside
`Core`. It's unclear from the public docs whether a brand-new platform id
like `openjazz` (there's no existing Jazz Jackrabbit platform in the
catalogue) is derived automatically from `Platforms/openjazz.json` in the
release ZIP, or whether a maintainer has to add it by hand after the issue
is filed. The OpenAPI spec doesn't say, and there's no visible schema for
`platforms.json` submission. Flagging this rather than guessing.

## Is a GitHub release required, and what must the asset be named

A GitHub release with an attached ZIP is **not strictly required**: the
issue template's "Path" option exists precisely for cores distributed as a
plain file in a repository instead. But it's the default path and the one
that fits us: our `releases/` directory is git-ignored build output, so we
are not going to check a ZIP into git.

The asset name is **not mandated**. Checked two real cores' latest releases
via the GitHub API:

- `agg23/openfpga-arduboy`: tag `0.9.0`, one asset:
  `agg23.Arduboy.0.9.0.zip`.
- `spiritualized1997/openFPGA-GB-GBC`: tag `v1.3.0`, two assets:
  `Spiritualized_GBC_1.3.0_2022_08_25.zip` and
  `Spiritualized_GB_1.3.0_2022_08_25.zip`.

Naming conventions differ between cores; the constraint that matters is
**how many assets the release has**, not what they're called. A release with
one asset needs no configuration; a release with more than one needs the
Asset Filter regex to pick the right one. Our `make package` always produces
exactly one ZIP (`openjazz-v<version>.zip`, from `core.json`'s version), so
no filter would be needed. GitHub's own "latest release" semantics (which
the catalogue almost certainly reads from, given the API's `download_url`
examples point at `releases/download/...` URLs) exclude drafts and
pre-releases; this is standard GitHub behaviour, not something the
openFPGA Library docs state explicitly, so treat it as a reasonable
assumption, not a confirmed rule.

## Versioning

The patch number counts commits: commit N of this repository carries version
`0.1.N`. Every commit that ships bumps it in two places, which must agree.
`version` in `dist/openjazz/Cores/negenii.OpenJazz/core.json` names the
release ZIP and shows up in the Pocket's core list, and `OJ_VERSION` in
`src/openjazz/Makefile` is what the game prints. N is the number of commits
since the import commit that opens the history: `git rev-list --count
b61859c5b4ec..HEAD`, plus one for the commit you are about to write.

## Steps, in order

1. Push this repository to `https://github.com/negenii/openJazz_pocket`
   (checked via the GitHub API: 404 on `repos/negenii/openJazz_pocket`, so
   the name is still free).
2. Tag and cut a GitHub release, with `releases/pocket/openjazz-v<version>.zip`
   (built by `make package`, see the root README) attached as its only
   asset.
3. Open an issue on `openfpga-library/analogue-pocket` using the "Add Core"
   template, giving the repository as `negenii/openJazz_pocket`. Leave
   Asset Filter and Path blank (single-asset GitHub release).
4. Wait for a maintainer to process the issue. Whether the new `openjazz`
   platform entry needs separate maintainer action (see above) is unknown
   until this happens.

## Sources

- <https://github.com/openfpga-library/analogue-pocket> (README, "Adding a
  new core")
- <https://raw.githubusercontent.com/openfpga-library/analogue-pocket/main/.github/ISSUE_TEMPLATE/ADD-CORE.yml>
- <https://openfpga-library.github.io/analogue-pocket/api/swagger/> and
  `.../api/v3/openapi.yaml`
- `https://api.github.com/repos/openfpga-cores-inventory/analogue-pocket`
  (redirect confirming the org rename) and
  `https://api.github.com/repos/joshcampbell191/openfpga-cores-inventory`
  (confirms that repo is archived)
- <https://github.com/mattpannella/pupdate> (README)
- <https://github.com/neil-morrison44/pocket-sync> (README)
- `https://api.github.com/repos/agg23/openfpga-arduboy/releases/latest` and
  `https://api.github.com/repos/spiritualized1997/openFPGA-GB-GBC/releases/latest`
  (real release/asset naming)
- `https://api.github.com/repos/negenii/openJazz_pocket` (404, repo not yet
  pushed)
