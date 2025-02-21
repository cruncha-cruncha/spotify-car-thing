// After updating all definitions, rename this file to "secrets.h"

#define SSID ""     // Change this to your WiFi name
#define PASSWORD "" // Change this to your WiFi password

// Sign up for a Spotify Developer account
// Create a new app through `https://developer.spotify.com/dashboard`, set the callback URI to `http://localhost`
// Go to app settings, copy `ClientID` and `ClientSecret`

#define CLIENT_ID ""
#define CLIENT_SECRET ""

// paste your ClientId into this URL: `https://accounts.spotify.com/authorize?client_id=<ClientID>&response_type=code&redirect_uri=http://localhost&scope=user-read-playback-state%20user-modify-playback-state`
// visit that URL in the browser, follow the directions
// it should bring you to a http://localhost page that doesn't load, but the new URL should have a 'code' param, copy that
// Use Postman or Insomnia to make a call to `https://accounts.spotify.com/api/token` with the header `Content-type: application/x-www-form-urlencoded` and following body:
/*
{
    "grant_type": "authorization_code",
    "code": "<code>",
    "redirect_uri": "http://localhost",
    "client_id": "<ClientID>",
    "client_secret": "<ClientSecret>"
}
*/
// Copy the `refresh_token` from the response

#define REFRESH_TOKEN ""

//  (Spotify instructions copied from https://github.com/espired/esp32-spotify-controller)


