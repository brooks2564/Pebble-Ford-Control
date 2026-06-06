/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;
/******/
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(1);
	module.exports = __webpack_require__(2);


/***/ }),
/* 1 */
/***/ (function(module, exports) {

	/**
	 * Copyright 2024 Google LLC
	 *
	 * Licensed under the Apache License, Version 2.0 (the "License");
	 * you may not use this file except in compliance with the License.
	 * You may obtain a copy of the License at
	 *
	 *     http://www.apache.org/licenses/LICENSE-2.0
	 *
	 * Unless required by applicable law or agreed to in writing, software
	 * distributed under the License is distributed on an "AS IS" BASIS,
	 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	 * See the License for the specific language governing permissions and
	 * limitations under the License.
	 */
	
	(function(p) {
	  if (!p === undefined) {
	    console.error('Pebble object not found!?');
	    return;
	  }
	
	  // Aliases:
	  p.on = p.addEventListener;
	  p.off = p.removeEventListener;
	
	  // For Android (WebView-based) pkjs, print stacktrace for uncaught errors:
	  if (typeof window !== 'undefined' && window.addEventListener) {
	    window.addEventListener('error', function(event) {
	      if (event.error && event.error.stack) {
	        console.error('' + event.error + '\n' + event.error.stack);
	      }
	    });
	  }
	
	})(Pebble);


/***/ }),
/* 2 */
/***/ (function(module, exports) {

	// Ford Control — PebbleKit JS
	// Bridges watch AppMessage <-> Ford Control server on Render
	
	var SERVER_URL = 'https://ford-control-server.onrender.com';  // Update after Render deploy
	var API_KEY    = 'changeme';  // Must match FORD_API_KEY env var on server
	
	var KEY_ACTION          = 0;
	var KEY_RESPONSE        = 1;
	var KEY_IS_LOCKED       = 2;
	var KEY_IS_RUNNING      = 3;
	var KEY_FUEL_LEVEL      = 4;
	var KEY_OIL_LIFE        = 5;
	var KEY_TIRE_FL         = 6;
	var KEY_TIRE_FR         = 7;
	var KEY_TIRE_RL         = 8;
	var KEY_TIRE_RR         = 9;
	var KEY_MODEL_NAME      = 10;
	var KEY_ERROR_MSG       = 11;
	var KEY_CLIMATE_TEMP    = 12;
	var KEY_CLIMATE_SEAT    = 13;
	var KEY_CLIMATE_WHEEL   = 14;
	var KEY_CLIMATE_DEFROST = 15;
	var KEY_APPLY_CLIMATE   = 16;
	
	var ACTION_LOCK        = 0;
	var ACTION_UNLOCK      = 1;
	var ACTION_START       = 2;
	var ACTION_STOP        = 3;
	var ACTION_FIND        = 4;
	var ACTION_GET_STATUS  = 5;
	var ACTION_GET_INFO    = 6;
	var ACTION_CLIMATE     = 7;
	
	function apiRequest(method, path, body, callback) {
	  var xhr = new XMLHttpRequest();
	  xhr.open(method, SERVER_URL + path);
	  xhr.setRequestHeader('Content-Type', 'application/json');
	  xhr.setRequestHeader('X-API-Key', API_KEY);
	  xhr.timeout = 15000;
	
	  xhr.onload = function() {
	    if (xhr.status === 200) {
	      try {
	        callback(null, JSON.parse(xhr.responseText));
	      } catch (e) {
	        callback('Parse error');
	      }
	    } else {
	      callback('HTTP ' + xhr.status);
	    }
	  };
	  xhr.onerror   = function() { callback('Network error'); };
	  xhr.ontimeout = function() { callback('Timeout'); };
	
	  xhr.send(body ? JSON.stringify(body) : null);
	}
	
	function sendToWatch(payload) {
	  Pebble.sendAppMessage(payload, function() {}, function(e) {
	    console.log('Send failed: ' + JSON.stringify(e));
	  });
	}
	
	function sendError(msg) {
	  var payload = {};
	  payload[KEY_ERROR_MSG] = msg.substring(0, 60);
	  sendToWatch(payload);
	}
	
	function sendStatus(data) {
	  var payload = {};
	  payload[KEY_IS_LOCKED]  = data.is_locked  ? 1 : 0;
	  payload[KEY_IS_RUNNING] = data.is_running ? 1 : 0;
	  if (data.model_name) payload[KEY_MODEL_NAME] = data.model_name;
	  if (data.climate_temp)    payload[KEY_CLIMATE_TEMP]    = data.climate_temp;
	  if (data.climate_seat)    payload[KEY_CLIMATE_SEAT]    = data.climate_seat ? 1 : 0;
	  if (data.climate_wheel)   payload[KEY_CLIMATE_WHEEL]   = data.climate_wheel ? 1 : 0;
	  if (data.climate_defrost) payload[KEY_CLIMATE_DEFROST] = data.climate_defrost ? 1 : 0;
	  sendToWatch(payload);
	}
	
	function sendInfo(data) {
	  var payload = {};
	  if (data.fuel_level != null) payload[KEY_FUEL_LEVEL] = data.fuel_level;
	  if (data.oil_life   != null) payload[KEY_OIL_LIFE]   = data.oil_life;
	  if (data.tire_fl    != null) payload[KEY_TIRE_FL]     = data.tire_fl;
	  if (data.tire_fr    != null) payload[KEY_TIRE_FR]     = data.tire_fr;
	  if (data.tire_rl    != null) payload[KEY_TIRE_RL]     = data.tire_rl;
	  if (data.tire_rr    != null) payload[KEY_TIRE_RR]     = data.tire_rr;
	  sendToWatch(payload);
	}
	
	Pebble.addEventListener('ready', function() {
	  console.log('PebbleKit JS ready');
	});
	
	Pebble.addEventListener('appmessage', function(e) {
	  var payload = e.payload;
	  var action = payload[KEY_ACTION];
	
	  if (action === ACTION_GET_STATUS) {
	    apiRequest('GET', '/status', null, function(err, data) {
	      if (err) { sendError(err); return; }
	      sendStatus(data);
	    });
	
	  } else if (action === ACTION_GET_INFO) {
	    apiRequest('GET', '/info', null, function(err, data) {
	      if (err) { sendError(err); return; }
	      sendInfo(data);
	    });
	
	  } else if (action === ACTION_LOCK) {
	    apiRequest('POST', '/lock', null, function(err, data) {
	      if (err) { sendError(err); return; }
	      sendStatus(data);
	    });
	
	  } else if (action === ACTION_UNLOCK) {
	    apiRequest('POST', '/unlock', null, function(err, data) {
	      if (err) { sendError(err); return; }
	      sendStatus(data);
	    });
	
	  } else if (action === ACTION_START) {
	    apiRequest('POST', '/start', null, function(err, data) {
	      if (err) { sendError(err); return; }
	      sendStatus(data);
	    });
	
	  } else if (action === ACTION_STOP) {
	    apiRequest('POST', '/stop', null, function(err, data) {
	      if (err) { sendError(err); return; }
	      sendStatus(data);
	    });
	
	  } else if (action === ACTION_FIND) {
	    apiRequest('POST', '/find', null, function(err, data) {
	      if (err) { sendError(err); return; }
	      sendStatus(data);
	    });
	
	  } else if (action === ACTION_CLIMATE) {
	    var body = {
	      temp:    payload[KEY_CLIMATE_TEMP]    || 72,
	      seat:    payload[KEY_CLIMATE_SEAT]    ? true : false,
	      wheel:   payload[KEY_CLIMATE_WHEEL]   ? true : false,
	      defrost: payload[KEY_CLIMATE_DEFROST] ? true : false
	    };
	    apiRequest('POST', '/climate', body, function(err, data) {
	      if (err) { sendError(err); return; }
	      sendStatus(data);
	    });
	  }
	});


/***/ })
/******/ ]);
//# sourceMappingURL=pebble-js-app.js.map