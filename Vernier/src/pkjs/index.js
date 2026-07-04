Pebble.addEventListener("ready", function() {
  console.log("JS ready");
});

Pebble.addEventListener("showConfiguration", function() {
  Pebble.openURL("https://gillesblaataap.github.io/watchfaces/Vernier/settings.html");
});

Pebble.addEventListener("webviewclosed", function(e) {
  if (!e || !e.response) return;

  var config = JSON.parse(decodeURIComponent(e.response));

  Pebble.sendAppMessage({
    "DarkMode": config.DarkMode ? 1 : 0
  });
});