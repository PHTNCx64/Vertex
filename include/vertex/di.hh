//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#ifdef _
#undef _
#endif


#include <boost/di.hpp>
#include <vertex/configuration/settings.hh>
#include <vertex/configuration/pluginconfig.hh>
#include <vertex/runtime/loader.hh>
#include <vertex/log/log.hh>
#include <vertex/gui/iconmanager/iconmanager.hh>
#include <vertex/language/language.hh>
#include <vertex/scanner/memoryscanner/memoryscanner.hh>
#include <vertex/io/io.hh>
#include <vertex/event/eventbus.hh>
#include <vertex/thread/threaddispatcher.hh>

namespace Vertex::DI
{
    [[nodiscard]] inline auto create_injector()
    {
        namespace di = boost::di;
        return di::make_injector(
        di::bind<Configuration::ISettings>()
            .to<Configuration::Settings>()
            .in(di::singleton),

        di::bind<Configuration::IPluginConfig>()
            .to<Configuration::PluginConfig>()
            .in(di::singleton),

        di::bind<Log::ILog>()
            .to<Log::Log>()
            .in(di::singleton),

        di::bind<Gui::IIconManager>()
            .to<Gui::IconManager>()
            .in(di::singleton),

        di::bind<Language::ILanguage>()
            .to<Language::Language>()
            .in(di::singleton),

        di::bind<IO::IIO>()
            .to<IO::IO>()
            .in(di::singleton),

        di::bind<Event::EventBus>().in(di::singleton),

        di::bind<Scanner::IMemoryScanner>()
            .to<Scanner::MemoryScanner>()
            .in(di::singleton),

        di::bind<Runtime::ILoader>()
            .to<Runtime::Loader>()
            .in(di::singleton),

        di::bind<Thread::IThreadDispatcher>()
            .to<Thread::ThreadDispatcher>()
            .in(di::singleton));
    }
}
