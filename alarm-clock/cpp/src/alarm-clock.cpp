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

/**
 * @file
 * @ingroup howtocode
 * @brief Alarm clock in C++
 *
 * This alarm-clock application is part of a series of how-to Intel IoT code
 * sample exercises using the Intel® IoT Developer Kit, Intel® Edison board,
 * cloud platforms, APIs, and other technologies.
 *
 * @hardware Sensors used:\n
 * Grove Rotary Angle Sensor\n
 * Grove Button\n
 * Grove Buzzer\n
 * Grove RGB LCD\n
 *
 * @cc
 * @cxx -std=c++1y
 * @ld -lupm-i2clcd -lupm-buzzer -lpaho-mqtt3cs -lupm-grove -lboost_system -lboost_thread -lboost_filesystem -lboost_date_time -lpthread -lcurl
 *
 * Additional source files required to build this example:
 * @req datastore.cpp
 * @req mqtt.cpp
 *
 * @date 09/22/2016
 */

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sstream>
#include <thread>
#include <ctime>

#include "../lib/crow/crow_all.h"
#include "index_html.h"
#include "styles_css.h"

#include "kits.h"
#if INTEL_IOT_KIT == DFROBOTKIT
#include "dfrobotkit.hpp"
#else
#include "grovekit.hpp"
#endif

#include "../lib/restclient-cpp/include/restclient-cpp/restclient.h"

#include "datastore.h"
#include "mqtt.h"

// Is the alarm currently ringing?
bool alarmRinging = false;

// The time that the alarm is currently set for
std::time_t alarmTime ;

// Useful function used for checking how much time is left before alarm
double countdown(std::time_t& target) {
  time_t rawtime;
  struct tm* timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  return std::difftime(mktime(timeinfo), target);
}

// How much time from when the alarm went off, to when user pushed button
double elapsed(std::time_t& target) {
  return countdown(target);
}

// Is it time to get up now?
bool time_for_alarm(std::time_t& alarm) {
  double remaining = countdown(alarm);

  if (remaining > 0 && remaining < 5 && !alarmRinging) {
    return true;
  } else return false;
}

// Call datastore/mqtt server to log how long it took to wake up today
void log_wakeup() {
  double duration = elapsed(alarmTime);
  std::cerr << "Alarm duration: " << std::to_string(duration) << std::endl;

  std::stringstream text;
  text << "{\"value\": \"" << std::to_string(duration) << "\"}";

  log_mqtt(text.str());
  log_datastore(text.str());
}

// Call weather underground API to get current weather conditions
std::string get_weather() {
  if (!getenv("API_KEY")) {
    std::cerr << "Weather Underground API_KEY not configured." << std::endl;
    return "";
  }

  std::string location = getenv("LOCATION") ? getenv("LOCATION") : "CA/San_Francisco";
  std::stringstream url;
  url << "http://api.wunderground.com/api/" << getenv("API_KEY") << "/conditions/q/" << location << ".json";

  RestClient::headermap headers;
  headers["Accept"] = "application/json";

  RestClient::response r = RestClient::get(url.str(), headers);
  if (r.code == 200) {
    auto x = crow::json::load(r.body);
    std::string result(x["current_observation"]["weather"].s());
    std::cout << result << std::endl;
    return result;
  } else {
    std::cerr << "Unable to get weather data" << std::endl;
    return "error";
  }
}

// Function called by worker thread for device communication
void runner(Devices& devices, std::time_t& alarmTime) {
  for (;;) {
    devices.display_time();
    devices.adjust_brightness();

    if (time_for_alarm(alarmTime)) {
      alarmRinging = true;
      devices.message(get_weather());
    }

    if (alarmRinging) {
      if ( devices.button->value() ) {
        alarmRinging = false;
        devices.stop_buzzing();

        // log how long it took to wake up
        log_wakeup();

        // set alarm time to tomorrow
        struct tm* timeinfo;
        timeinfo = localtime(&alarmTime);
        timeinfo->tm_mday++;
        alarmTime = mktime(timeinfo);
        std::cerr << "New alarm time: " << alarmTime << std::endl;

      } else {
        devices.start_buzzing();
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}

Devices devices;

// Exit handler for program
void exit_handler(int param)
{
  devices.cleanup();
  exit(1);
}

// The main function for the example program
int main() {
  // Handles ctrl-c or other orderly exits
  signal(SIGINT, exit_handler);

  // check that we are running on Galileo or Edison
  mraa_platform_t platform = mraa_get_platform_type();
  if ((platform != MRAA_INTEL_GALILEO_GEN1) &&
    (platform != MRAA_INTEL_GALILEO_GEN2) &&
    (platform != MRAA_INTEL_EDISON_FAB_C)) {
    std::cerr << "ERROR: Unsupported platform" << std::endl;
    return MRAA_ERROR_INVALID_PLATFORM;
  }

  // create and initialize UPM devices
  devices.init();

  // start worker thread for device communication
  std::thread t1(runner, std::ref(devices), std::ref(alarmTime));

  // define web server & routes
  crow::SimpleApp app;

  CROW_ROUTE(app, "/")
  ([](const crow::request& req) {
    bool timeChanged = false;
    std::time_t newTime = std::time(NULL);
    struct tm* timeinfo;
    timeinfo = localtime(&newTime);

    if(req.url_params.get("hour") != nullptr) {
      double hour = boost::lexical_cast<double>(req.url_params.get("hour"));
      timeinfo->tm_hour = hour;
      timeChanged = true;
    }

    if(req.url_params.get("minute") != nullptr) {
      double minute = boost::lexical_cast<double>(req.url_params.get("minute"));
      timeinfo->tm_min = minute;
      timeChanged = true;
    }

    if(req.url_params.get("second") != nullptr) {
      double second = boost::lexical_cast<double>(req.url_params.get("second"));
      timeinfo->tm_sec = second;
      timeChanged = true;
    }

    if (timeChanged) {
      alarmTime = mktime(timeinfo);
      std::cerr << "Set alarm time: " << alarmTime << std::endl;
    }

    std::stringstream text;
    text << index_html;
    return text.str();
  });

  CROW_ROUTE(app, "/alarm.json")
  ([]() {
    struct tm* timeinfo;
    timeinfo = localtime(&alarmTime);

    crow::json::wvalue a;
    a["hours"] = timeinfo->tm_hour;
    a["minutes"] = timeinfo->tm_min;
    a["seconds"] = timeinfo->tm_sec;
    return a;
  });

  CROW_ROUTE(app, "/styles.css")
  ([]() {
    std::stringstream text;
    text << styles_css;
    return text.str();
  });

  // start web server
  app.port(3000).multithreaded().run();

  // wait forever for the thread to exit
  t1.join();

  return MRAA_SUCCESS;
}
