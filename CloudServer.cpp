//============================================================================
// Name        : CloudServer.cpp
// Author      : Pravesh Kumar
// Version     :
// Copyright   : Copyright Reserve with Technolabs
// Description : Hello World in C, Ansi-style
//============================================================================

//
// WebSocketServer.cpp
//
// $Id: //poco/1.4/Net/samples/WebSocketServer/src/WebSocketServer.cpp#1 $
//
// This sample demonstrates the WebSocket class.
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//
#define SAMPLE_INTERVAL  (1 * 1000 * 1000)
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/Format.h"
//Custom
#include "JSONUtil.hpp"
#include <fstream>
#include <cstring>
#include <limits>
#include <iostream>
#include <sstream>

using namespace std;
using Poco::Net::ServerSocket;
using Poco::Net::WebSocket;
using Poco::Net::WebSocketException;
using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPRequestHandlerFactory;
using Poco::Net::HTTPServer;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPServerResponse;
using Poco::Net::HTTPServerParams;
using Poco::Timestamp;
using Poco::ThreadPool;
using Poco::Util::ServerApplication;
using Poco::Util::Application;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;

class PageRequestHandler: public HTTPRequestHandler
/// Return a HTML document with some JavaScript creating
/// a WebSocket connection.
{
public:
	void handleRequest(HTTPServerRequest& request,
			HTTPServerResponse& response) {
		response.setChunkedTransferEncoding(true);
		response.setContentType("text/html");
		std::ostream& ostr = response.send();
		ostr << "<html>";
		ostr << "<head>";
		ostr << "<title>WebSocketServer</title>";
		ostr << "<script type=\"text/javascript\">";
		ostr << "function WebSocketTest()";
		ostr << "{";
		ostr << "  if (\"WebSocket\" in window)";
		ostr << "  {";
		ostr << "    var ws = new WebSocket(\"ws://"
				<< request.serverAddress().toString() << "/ws\");";
		ostr << "    ws.onopen = function()";
		ostr << "      {";
		ostr << "        ws.send(\"Hello, world!\");";
		ostr << "      };";
		ostr << "    ws.onmessage = function(evt)";
		ostr << "      { ";
		ostr << "        var msg = evt.data;";
		ostr << "        alert(\"Message received: \" + msg);";
		ostr << "        ws.close();";
		ostr << "      };";
		ostr << "    ws.onclose = function()";
		ostr << "      { ";
		ostr << "        alert(\"WebSocket closed.\");";
		ostr << "      };";
		ostr << "  }";
		ostr << "  else";
		ostr << "  {";
		ostr << "     alert(\"This browser does not support WebSockets.\");";
		ostr << "  }";
		ostr << "}";
		ostr << "</script>";
		ostr << "</head>";
		ostr << "<body>";
		ostr << "  <h1>WebSocket Server</h1>";
		ostr
				<< "  <p><a href=\"javascript:WebSocketTest()\">Run WebSocket Script</a></p>";
		ostr << "</body>";
		ostr << "</html>";
	}
};

class WebSocketRequestHandler: public HTTPRequestHandler
/// Handle a WebSocket connection.
{
private :
	JSONUtil jsonUtil;
public:

	std::string convertToString(int x){
		ostringstream temp;  //temp as in temporary
		temp << x;
		return temp.str();
	}

	void loadFile(unsigned int start_poistion, const char* filepath, Poco::JSON::Object &json) {
		fstream file(filepath);

		file.seekg(std::ios::beg);
		unsigned int n = start_poistion;
		unsigned int m = 10 + n;

		for (unsigned int i = 0; i < n; ++i) {
			file.ignore(i, '\n');
		}

		jsonUtil.updateJson(json, "start", convertToString(start_poistion));
		string str;
		int count = n;
		while (std::getline(file, str) && (n < m)) {
			jsonUtil.updateJson(json, convertToString(count), str);
			count++;
			n++;
		}
		jsonUtil.updateJson(json, "next", convertToString(count));
	}



	void handleRequest(HTTPServerRequest& request,
			HTTPServerResponse& response) {
		Application& app = Application::instance();
		try {
			WebSocket ws(request, response);
			app.logger().information("WebSocket connection established.");

			char sndBuffer[4048];
			int flags;
			int n;

			do {
				char buffer[4048];
				memset(&buffer[0], 0, sizeof(buffer));
				n = ws.receiveFrame(buffer, sizeof(buffer), flags);
				std::string str(buffer);
				std::cout << "Request : "<< str << std::endl;
				//main logic start
				Poco::JSON::Object json;
				loadFile(12, "/home/ubuntu/error.log", json);
				strcpy(sndBuffer, jsonUtil.createJsonString(json).c_str());
				//main logic end
				ws.sendFrame(sndBuffer, sizeof(sndBuffer), flags);
				usleep(SAMPLE_INTERVAL);
			} while (n > 0
					&& (flags & WebSocket::FRAME_OP_BITMASK)
							!= WebSocket::FRAME_OP_CLOSE);
			app.logger().information("WebSocket connection closed.");
		} catch (WebSocketException& exc) {
			app.logger().log(exc);
			switch (exc.code()) {
			case WebSocket::WS_ERR_HANDSHAKE_UNSUPPORTED_VERSION:
				response.set("Sec-WebSocket-Version",
						WebSocket::WEBSOCKET_VERSION);
				// fallthrough
			case WebSocket::WS_ERR_NO_HANDSHAKE:
			case WebSocket::WS_ERR_HANDSHAKE_NO_VERSION:
			case WebSocket::WS_ERR_HANDSHAKE_NO_KEY:
				response.setStatusAndReason(HTTPResponse::HTTP_BAD_REQUEST);
				response.setContentLength(0);
				response.send();
				break;
			}
		}
	}
};

class RequestHandlerFactory: public HTTPRequestHandlerFactory {
public:
	HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& request) {
		Application& app = Application::instance();
		app.logger().information(
				"Request from " + request.clientAddress().toString() + ": "
						+ request.getMethod() + " " + request.getURI() + " "
						+ request.getVersion());

		for (HTTPServerRequest::ConstIterator it = request.begin();
				it != request.end(); ++it) {
			app.logger().information(it->first + ": " + it->second);
		}

		if (request.find("Upgrade") != request.end()
				&& Poco::icompare(request["Upgrade"], "websocket") == 0)
			return new WebSocketRequestHandler;
		else
			return new PageRequestHandler;
	}
};

class WebSocketServer: public Poco::Util::ServerApplication
/// To test the WebSocketServer you can use any web browser (http://localhost:9980/).
{
public:
	WebSocketServer() :
			_helpRequested(false) {
	}

	~WebSocketServer() {
	}

protected:
	void initialize(Application& self) {
		loadConfiguration(); // load default configuration files, if present
		ServerApplication::initialize(self);
	}

	void uninitialize() {
		ServerApplication::uninitialize();
	}

	void defineOptions(OptionSet& options) {
		ServerApplication::defineOptions(options);

		options.addOption(
				Option("help", "h",
						"display help information on command line arguments").required(
						false).repeatable(false));
	}

	void handleOption(const std::string& name, const std::string& value) {
		ServerApplication::handleOption(name, value);

		if (name == "help")
			_helpRequested = true;
	}

	void displayHelp() {
		HelpFormatter helpFormatter(options());
		helpFormatter.setCommand(commandName());
		helpFormatter.setUsage("OPTIONS");
		helpFormatter.setHeader(
				"A sample HTTP server supporting the WebSocket protocol.");
		helpFormatter.format(std::cout);
	}

	int main(const std::vector<std::string>& args) {
		if (_helpRequested) {
			displayHelp();
		} else {
			// get parameters from configuration file
			unsigned short port = (unsigned short) config().getInt(
					"WebSocketServer.port", 9980);

			// set-up a server socket
			ServerSocket svs(port);
			// set-up a HTTPServer instance
			HTTPServer srv(new RequestHandlerFactory, svs,
					new HTTPServerParams);
			// start the HTTPServer
			srv.start();
			// wait for CTRL-C or kill
			waitForTerminationRequest();
			// Stop the HTTPServer
			srv.stop();
		}
		return Application::EXIT_OK;
	}

private:
	bool _helpRequested;
};

POCO_SERVER_MAIN(WebSocketServer)
