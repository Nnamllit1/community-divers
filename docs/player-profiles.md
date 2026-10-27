# Community player profiles

The launcher can sign in to a community profile and show its live name, XP,
successful mission count and saved-loadout entry count. These are the selected
community's records, separate from official accounts and progression. Game launch,
loadout editing, self-service registration and gameplay integration remain in
development.

## Sign in

1. Ask the community operator for your player credential JSON file. Operators
   create it with `CommunityDiversService player DATA --name "Name" --output
   player.credential.json`. The file is the account credential; it is not a mod
   manifest or an official Steam/Arrowhead login.
2. Add/select that community in **Communities**, then press **Check selected**.
3. Open **Campaign**. In **Community profile > Sign-in options**, choose
   **Import player credential** and select the file. No token is pasted into chat or shown in the launcher.
4. Leave **Remember sign-in** checked to save it for the current Windows account,
   or uncheck it for this launcher session only.

The launcher validates the file's community ID, player ID and token format,
checks the endpoint's public identity again, and reads `/v1/me` with the player
credential. The returned player ID must match. A failed import does not replace
an existing saved credential. The original JSON file is not moved or deleted.

Remembered sign-ins use
[Windows Credential Manager](https://learn.microsoft.com/en-us/windows/win32/api/wincred/nf-wincred-credwritew),
scoped to the launcher's data directory, canonical community URL and community ID.
Tokens are not stored in `launcher.json`, copied into mod profiles, sent with
public status/campaign checks or written to launcher logs. They are used only for
explicit player-profile requests to that community. HTTP is allowed only on
loopback for local development; remote endpoints require HTTPS. Profile requests
reject redirects, automatic Windows authentication, cookies and oversized replies.

After restarting, check the saved community connection again to restore its
remembered sign-in. **Use saved sign-in** retries it, and **Refresh profile** reads
the current service record. Moving the launcher data directory or changing the
community URL requires importing the credential again.

## Sign out

**Sign out and forget** clears the active profile and removes this community's
saved credential from the current Windows account. It also invalidates a sign-in
request still in flight. Switching communities clears the displayed profile and
discards replies for the old connection. A delayed request cannot sign you back
in after sign-out or after switching away and back.

A successful import with **Remember sign-in** unchecked also removes a previous
saved sign-in for that community. Forgetting a local sign-in does not revoke the
credential at the service, erase community progression or delete the original
credential file. Server-side rotation/revocation is not implemented yet.

Widget tests exercise importing a selected file. Native file-picker input and
desktop input/DPI transitions remain unverified. The current profile path does not establish a working game
adapter or authorize game launch.
