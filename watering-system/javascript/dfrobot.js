/*
* Copyright (c) 2015 - 2016 Intel Corporation.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
* LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
* OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
* WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

"use strict";

var exports = module.exports = {};

// The program is using the `mraa` module
// to communicate directly with the digital
// pin used to turn on/off the water pump
var mraa = require("mraa");

// Initialize the DFRobot hardware devices
var moisture = new (require("jsupm_grovemoisture").GroveMoisture)(1), // A1
    pump = new mraa.Gpio(16); // aka A2

// Set GPIO direction to output
pump.dir(mraa.DIR_OUT);

exports.init = function(config) {
  return;
}

// The program handles events generated by the various connected
// hardware devices using the Node.js built-in `events` module
var events = new (require("events").EventEmitter)();
exports.events = events;

// Check that water is flowing
exports.checkFlowOn = function() {
  setTimeout(function() {
    if (moistureValue() < 1) { events.emit("alert"); }
  }, 2000);
}

// Without flow sensor, cannot check that water isn't flowing
exports.checkFlowOff = function() {
  return;
}

// Turns on the water
exports.turnOn = function() {
  var that = this;    
  pump.write(1);

  // check flow started after 10 seconds
  setTimeout(that.checkFlowOn, 10000);
}

// Turns off the water
exports.turnOff = function() {
  pump.write(0);
}

// current moisture sensor reading
exports.moistureValue = function() {
  return moisture.value();
}
