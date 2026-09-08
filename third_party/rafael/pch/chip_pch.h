/*
 *    Copyright (c) 2026 Rafael Micro
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *  Precompiled header for the RT58x Matter build.
 */

#include <app-common/zap-generated/cluster-objects.h>
#include <app/AppConfig.h>
#include <app/InteractionModelEngine.h>
#include <app/server-cluster/DefaultServerCluster.h>
#include <data-model-providers/codegen/ClusterIntegration.h>
#include <lib/core/CHIPError.h>
#include <lib/core/TLV.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/CHIPDeviceLayer.h>
