import ziti from "../ziti.js";
import test from "node:test";
import assert from "node:assert";
import { setTimeout as delay } from 'node:timers/promises';

test('setLogger test', async () => {
    const messages = [];
    ziti.setLogger(
        (msg) => {
            messages.push(msg);
        }
    );

    ziti.setLogLevel(5);
    await delay(1000);
    assert(messages.length > 0, "Logger callback should have been called at least once");
    assert(messages.some(msg => msg.message.includes("set log level")),
        "Logger should contain log level set message"
    );
})




