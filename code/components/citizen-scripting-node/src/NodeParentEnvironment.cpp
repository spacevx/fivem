/*
 * This file is part of the Cfx project - https://cfx.re/
 *
 * See LICENSE in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"

#include <NodeParentEnvironment.h>
#include <console/Console.h>

#include <libplatform/libplatform.h>
#include "shared/RuntimeHelpers.h"
#include "UvLoopManager.h"

#ifndef _WIN32
#include <signal.h>
#endif

extern int g_argc;
extern char** g_argv;

#ifndef _WIN32
namespace
{
struct sigaction g_previousSegvAction;

void ForwardToPreviousSegvHandler(int signal, siginfo_t* info, void* ucontext)
{
	if (g_previousSegvAction.sa_flags & SA_SIGINFO)
	{
		g_previousSegvAction.sa_sigaction(signal, info, ucontext);
	}
	else
	{
		g_previousSegvAction.sa_handler(signal);
	}
}

bool IsDefaultDisposition(const struct sigaction& action)
{
	return !(action.sa_flags & SA_SIGINFO) && (action.sa_handler == SIG_DFL || action.sa_handler == SIG_IGN);
}

void ChainPreviousSegvHandler(const struct sigaction& previous)
{
	if (IsDefaultDisposition(previous))
	{
		return;
	}

	struct sigaction current = {};
	sigaction(SIGSEGV, nullptr, &current);

	if (current.sa_sigaction == previous.sa_sigaction)
	{
		return;
	}

	g_previousSegvAction = previous;
	node::RegisterSignalHandler(SIGSEGV, ForwardToPreviousSegvHandler);
}
}
#endif

namespace fx::nodejs
{
result_t NodeParentEnvironment::Initialize()
{
	// redirect initialization if it should be a simple node process
	if (IsStartNode())
	{
		return StartNode();
	}

	// initialize node process
	const static std::vector<std::string> args = {
		"FXServer.exe",
		"--trace-warnings",
		"--unhandled-rejections=warn",
		"--experimental-sqlite"
	};

#ifndef _WIN32
	struct sigaction previousSegvAction = {};
	sigaction(SIGSEGV, nullptr, &previousSegvAction);
#endif

	const auto result = node::InitializeOncePerProcess(args);

#ifndef _WIN32
	ChainPreviousSegvHandler(previousSegvAction);
#endif

	if (result->errors().size())
	{
		for (const auto& error : result->errors())
		{
			console::PrintError(_CFX_NAME_STRING(_CFX_COMPONENT_NAME), "Error while initializing node: %s\n", error);
		}
		return FX_E_INVALIDARG;
	}
	
	m_platform.reset(result->platform());
	
	m_isolate = node::NewIsolate(node::CreateArrayBufferAllocator(), uv_default_loop(), m_platform.get());
	if (!m_isolate)
	{
		console::PrintError(_CFX_NAME_STRING(_CFX_COMPONENT_NAME), "Error while initializing node: failed to create isolate\n");
		return FX_E_INVALIDARG;
	}
	
	SharedPushEnvironmentNoContext pushed(m_isolate);

	m_context.Reset(m_isolate, node::NewContext(m_isolate));

	m_initialized = true;
	return FX_S_OK;
}

void NodeParentEnvironment::Tick() const
{
	SharedPushEnvironmentNoContext pushed(m_isolate);
	m_platform->DrainTasks(m_isolate);
}

bool NodeParentEnvironment::IsStartNode() const
{
	// checking for node20 is enough, no need to check for start-node, that is only for compatibility
	// and to not change other files with each update
	// see NodeScriptRuntime.cpp -> node::CreateEnvironment to know where this parameter comes from
	return std::find_if(g_argv, g_argv + g_argc, [](char* arg)
		   {
			   return strcmp(arg, "--fork-node22") == 0;
		   })
		   != g_argv + g_argc;
}

result_t NodeParentEnvironment::StartNode()
{
	return node::Start(g_argc, g_argv);
}
}
