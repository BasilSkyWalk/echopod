# The gallery service

A small Cloudflare Worker behind two things on the theme site:

- **Send to the gallery** in the editor. The Worker checks the theme, then opens a pull request that adds `themes/<file>.toml`. The `themes` workflow checks and draws it as for any pull request and shows the picture in a comment, and merging it is what puts it in the gallery.
- **Likes** in the gallery. One like for each theme from each browser, counted in a D1 database.

The site works without it: with no service address the editor shows the by-hand steps and the gallery shows no hearts.

## What it accepts

| Request | What it does |
| --- | --- |
| `GET /likes` | The like count of every theme that has any. |
| `POST /likes/<file>` with `{"voter": "<id>"}` | Likes a theme the gallery lists. |
| `DELETE /likes/<file>` with `{"voter": "<id>"}` | Takes the like back. |
| `POST /themes` with `{"file", "toml", "token"}` | Opens the pull request and answers with its address. |

Anything that changes something must come from a page in `ALLOWED_ORIGINS`.

Its limits, all in `worker.js`:

- A theme source of at most 600,000 bytes, with a `name`, an `author` and a `description`.
- A file name of 1 to 23 of `a-z`, `0-9`, `-` and `_` that no theme in `themes` or `assets/themes` has, and that no open submission has.
- Three themes a day from one address (the `SENT_A_DAY` setting changes it), and at most 20 submissions waiting at once.
- Sixty likes a day from one address, five of them for any one theme.
- Sending needs a Turnstile token, checked with Cloudflare before anything is opened.

It only ever writes `themes/<file>.toml`, on a branch of its own named `gallery/<file>/<random>`.

## What it keeps

- For a like: the theme, a random id the browser made, the day, and the address it came from scrambled with `VOTER_SALT`.
- For a sent theme: the same scrambled address, the day, the file name and the pull request's number.

No names, accounts or plain addresses.

## Trying it on your own computer

```sh
node web/gallery-service/dev_server.mjs http://127.0.0.1:8770 8787
```

That runs the same code with a database in memory and a stand-in for GitHub, so nothing is opened anywhere; a sent theme can be read back at the address the editor links to. It lifts the three-a-day limit, and stopping it forgets everything. Build the site with `--service http://127.0.0.1:8787 --turnstile 1x00000000000000000000AA` (Cloudflare's always-pass test key) to point the pages at it.

The tests need only Node 22:

```sh
node web/gallery-service/test_gallery_service.mjs
node web/editor/test_gallery_service.mjs
```

## Setting it up

Once, by whoever runs the gallery. Wrangler runs with `npx wrangler` from this folder.

1. **A GitHub App.** Create one under the account that owns the repository, with repository permissions **Contents: read and write** and **Pull requests: read and write**, no webhook, installable only on that account. Install it on this repository alone. Note the App ID, generate a private key, and take the installation's number from the address of its settings page.
2. **A Turnstile widget.** Create one in the Cloudflare dashboard for the site's host name (`basilskywalk.github.io`). Note its site key and its secret key.
3. **The database.**

   ```sh
   npx wrangler d1 create echopod-gallery
   ```

   Put the id it prints into `database_id` in `wrangler.jsonc`, then make the tables:

   ```sh
   npx wrangler d1 migrations apply echopod-gallery --remote
   ```

4. **The settings.** Fill `GITHUB_APP_ID` and `GITHUB_INSTALLATION_ID` in `wrangler.jsonc`. Then the three secrets, each asked for when the command runs:

   ```sh
   npx wrangler secret put GITHUB_APP_PRIVATE_KEY
   npx wrangler secret put TURNSTILE_SECRET
   npx wrangler secret put VOTER_SALT
   ```

   The private key is the whole `.pem` file as GitHub gave it. `VOTER_SALT` is any long random text; changing it later forgets who has reached a daily limit, nothing else.
5. **Deploy.**

   ```sh
   npx wrangler deploy
   ```

6. **Tell the site.** In the repository's settings, under Actions variables, set `GALLERY_SERVICE` to the Worker's address and `GALLERY_TURNSTILE` to the Turnstile site key, then run the `themes` workflow. Clearing `GALLERY_SERVICE` and running it again turns both features off.

After the first theme is sent, check that the `themes` workflow ran on its pull request and that its picture appeared in a comment. The pictures are kept on a branch of their own, `gallery-previews`, which the workflow makes the first time.
