/* Utility to manage registration of plugins compiled statically
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include "static-plugin.hpp"

using namespace SpiceStreamingAgent;

const StaticPlugin *StaticPlugin::list = nullptr;

void StaticPlugin::InitAll(Agent& agent)
{
    for (const StaticPlugin* plugin = list; plugin; plugin = plugin->next) {
        plugin->init_func(&agent);
    }
}
