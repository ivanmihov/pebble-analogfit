//var Clay = require('pebble-clay');
//var clayConfig = require('./config');
//var clay = new Clay(clayConfig);

var config = {
  getConfig : function() {
    try {
    return JSON.parse(localStorage.getItem("options"));  
    } catch(e) {
      return {};
    }    
  },
  setConfig : function(options) {
    localStorage.setItem("options", JSON.stringify(options));
    Pebble.sendAppMessage("config.updated", options);
  },
};

Pebble.addEventListener('showConfiguration', function() {
	var configurationUrl = 'https://cloud.imihov.com/analog-fit/index.html';

  //console.log("showing configuration");
  var options = config.getConfig();
	//console.log(JSON.stringify(options));
  Pebble.openURL(configurationUrl + '?'+encodeURIComponent(JSON.stringify(options)));
});

Pebble.addEventListener('webviewclosed', function(e) {
  // Decode the user's preferences
  var configData = JSON.parse(decodeURIComponent(e.response));
	// Send to the watchapp via AppMessage
	var dict = {
  	'inverted_color': configData.inverted_color,
  	'steps_goal': configData.steps_goal,
		'show_seconds': configData.show_seconds,
		'custom_text': configData.custom_text.substring(0,18),
	};
	// Send to the watchapp
	Pebble.sendAppMessage(dict, function() {
		config.setConfig(dict);
  	//console.log('Config data sent successfully!' + JSON.stringify(dict));
	}, function(e) {
  	//console.log('Error sending config data!');
	});
});

// Battery level has changed
function chargeLevel (battery) {
	var batteryLevel = String(Math.round(battery.level*100));
	var dict = {
		'PhoneBattery': batteryLevel,
	};	
	Pebble.sendAppMessage(dict, function() {
  	//console.log('Message sent successfully: ' + JSON.stringify(dict));
		}, function(e) {
  	//console.log('Message failed: ' + JSON.stringify(e));
	});
}

// Battery charge status has changed
function chargeStatus (battery) {
	//console.log("Battery: " + ((battery.charging) ? 'Charging' : 'Discharging'));
	var charging_status = battery.charging ? 'Y' : 'N';
	Pebble.sendAppMessage('PhoneBattery', charging_status);
}

// Battery Init
function init (battery) {
	// Listen for changes in charging status
	//battery.addEventListener('chargingchange', function () {chargeStatus(battery);}, false);
	// Listen for changes in charge level
	battery.addEventListener('levelchange', function () {chargeLevel(battery);}, false);
	// Run both functions on page load
	chargeLevel(battery);
	//chargeStatus(battery);
}

function batteryStatusFailure() {
	//console.log("Phoneery function failed to successfully resolve the promise.");
}

Pebble.addEventListener('ready', function(e) {
	//console.log('PebbleKitReady!');
	//Pebble.showToast("PebbleKit JS Ready!");
	//console.log('userAgent: ' + (navigator.userAgent || "[invalid userAgent]"));

	// Test for old or new battery API
	if (navigator.battery) {
		init(navigator.battery);
	} else if (navigator.getBattery) {
		navigator.getBattery().then(function (newBattery){init(newBattery);}, batteryStatusFailure);
	} else {
		//console.log('None battery API. Only works on Android');
	}
});