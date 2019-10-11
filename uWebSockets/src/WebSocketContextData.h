/*
 * Authored by Alex Hultman, 2018-2019.
 * Intellectual property of third-party.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UWS_WEBSOCKETCONTEXTDATA_H
#define UWS_WEBSOCKETCONTEXTDATA_H

#include "f2/function2.hpp"
#include <string_view>

#include "WebSocketProtocol.h"
#include "TopicTreeDraft.h"

namespace uWS {

template <bool, bool> struct WebSocket;

/* todo: this looks identical to WebSocketBehavior, why not just std::move that entire thing in? */

template <bool SSL>
struct WebSocketContextData {
    /* The callbacks for this context */
    fu2::unique_function<void(WebSocket<SSL, true> *, std::string_view, uWS::OpCode)> messageHandler = nullptr;
    fu2::unique_function<void(WebSocket<SSL, true> *)> drainHandler = nullptr;
    fu2::unique_function<void(WebSocket<SSL, true> *, int, std::string_view)> closeHandler = nullptr;

    /* Settings for this context */
    size_t maxPayloadLength = 0;
    int idleTimeout = 0;

    /* There needs to be a maxBackpressure which will force close everything over that limit */
    size_t maxBackpressure = 0;

    /* Each websocket context has a topic tree for pub/sub */
    TopicTree topicTree;

    WebSocketContextData() : topicTree([this](Subscriber *s, std::string_view data) -> int {
        /* We rely on writing to regular asyncSockets */
        auto *asyncSocket = (AsyncSocket<SSL> *) s->user;

        auto [written, failed] = asyncSocket->write(data.data(), data.length());
        if (!failed) {
            asyncSocket->timeout(this->idleTimeout);
        } else {
            /* Note: this assumes we are not corked, as corking will swallow things and fail later on */

            /* Check if we now have too much backpressure (todo: don't buffer up before check) */
            if (asyncSocket->getBufferedAmount() > maxBackpressure) {
                asyncSocket->close();
            }
        }

        /* Reserved, unused */
        return 0;
    }) {
        /* bug: This should probably happen in both post and pre, esp for libuv */
        Loop::get()->addPostHandler([this](Loop *loop) {
            /* Commit pub/sub batches every loop iteration */
            topicTree.drain();
        });
    }
};

}

#endif // UWS_WEBSOCKETCONTEXTDATA_H
