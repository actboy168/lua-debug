#pragma once

#include "dbg_protocol.h"

class stdinput;

int run_launch(stdinput& io, vscode::rprotocol& init, vscode::rprotocol& req);
