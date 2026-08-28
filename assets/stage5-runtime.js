(function(){
    window.__DearOreUI__ = window.__DearOreUI__ || {};
    window.__DearOreUI__.protocolVersion = 1;
    window.__DearOreUI__.stage = 8;
    window.__DearOreUI__.contextId = "__DEAROREUI_CTX__";
    var hasFacet = (typeof engine !== 'undefined') && engine && engine.trigger;
    window.__DearOreUI__.bus = window.__DearOreUI__.bus || (function() {
        var pending = {};
        return {
            on: function(id, cb) { pending[id] = cb; },
            off: function(id) { delete pending[id]; },
            push: function(id, jsonString) {
                var cb = pending[id];
                if (!cb) return;
                delete pending[id];
                var resp = null;
                try { resp = JSON.parse(jsonString); } catch (e) { resp = { type: 'response', id: id, error: 1, payload: 'bad response json' }; }
                cb(resp);
            }
        };
    })();
    function dearOreUiFacetTrigger(params) {
        try {
            if (!hasFacet) return false;
            engine.trigger('facet:request', 'dearoreui', 'dearoreui', { params: params });
            return true;
        } catch (e) { return false; }
    }
    // 单发配额：success 时保持占用（每视图单次 facet 往返的硬约束），
    // 失败（通道未就绪 / 超时）时释放配额，允许一次补救，不再永久锁死。
    window.__DearOreUI__.dispatchUsed = false;
    window.__DearOreUI__.config = window.__DearOreUI__.config || {};
    window.__DearOreUI__.events = window.__DearOreUI__.events || (function(){ var listeners = {}; return { on:function(name, cb){ if(typeof cb !== 'function') return false; (listeners[name] = listeners[name] || []).push(cb); return true; }, off:function(name, cb){ var list=listeners[name]||[]; listeners[name]=list.filter(function(x){return x!==cb;}); }, push:function(name, payload){ (listeners[name]||[]).slice().forEach(function(cb){ try{cb(payload);}catch(e){} }); } }; })();
    window.__DearOreUI__.ipc = {
        isAvailable: function() { return hasFacet; },
        callHost: function(method, args, timeoutMs) {
            if (typeof timeoutMs !== 'number' || timeoutMs <= 0) {
                timeoutMs = (typeof window.__DearOreUI__.config.callHostTimeout === 'number' && window.__DearOreUI__.config.callHostTimeout > 0)
                    ? window.__DearOreUI__.config.callHostTimeout : 5000;
            }
            return new Promise(function(resolve, reject) {
                function fail(code, msg) { var e = new Error(msg || code); e.code = code; reject(e); }
                try {
                    if (!hasFacet) { fail('HostBridgeUnavailable', 'no facet channel'); return; }
                    if (window.__DearOreUI__.dispatchUsed) { fail('ViewDispatchAlreadyUsed', 'one dispatch per view already consumed'); return; }
                    var id = (window.__DearOreUI__.nextRequestId = (window.__DearOreUI__.nextRequestId || 0) + 1);
                    var request = {
                        type: 'request',
                        id: id,
                        ctx: Number(window.__DearOreUI__.contextId || 0),
                        method: method,
                        payload: (typeof args === 'string' ? args : JSON.stringify(args || {}))
                    };
                    window.__DearOreUI__.dispatchUsed = true; // 占用单发配额
                    var timer = setTimeout(function() {
                        window.__DearOreUI__.bus.off(id);
                        window.__DearOreUI__.dispatchUsed = false; // 超时失败 → 释放配额
                        fail('HostCallTimeout', 'host call timed out after ' + timeoutMs + 'ms');
                    }, timeoutMs);
                    window.__DearOreUI__.bus.on(id, function(resp) { clearTimeout(timer); resolve(resp); }); // 成功消费，保持占用
                    if (!dearOreUiFacetTrigger(JSON.stringify(request))) {
                        clearTimeout(timer);
                        window.__DearOreUI__.bus.off(id);
                        window.__DearOreUI__.dispatchUsed = false; // 通道未就绪 → 释放配额
                        fail('FacetUnavailable', 'facet dispatch failed');
                    }
                } catch (e) { if (e && !e.code) e.code = 'InternalError'; reject(e); }
            });
        },
        setTimeout: function(ms) { window.__DearOreUI__.config.callHostTimeout = ms; },
        report: function(msg) {
            try {
                if (window.__DearOreUI__ && window.__DearOreUI__.silent) return;
                dearOreUiFacetTrigger((typeof msg === 'string') ? msg : JSON.stringify(msg));
            } catch (e) {}
        },
        send: function(msg) { return this.report(msg); }
    };
    window.oreui = window.oreui || {};
    window.oreui.runtime = { protocolVersion: function(){ return window.__DearOreUI__.protocolVersion; }, contextId: function(){ return Number(window.__DearOreUI__.contextId || 0); }, isReady: function(){ return !!window.__DearOreUI__.ipc.isAvailable(); } };
    window.oreui.page = { contextId: function(){ return Number(window.__DearOreUI__.contextId || 0); } };
    window.oreui.host = { isAvailable: function(){ return window.__DearOreUI__.ipc.isAvailable(); }, call: function(method, args){ return window.__DearOreUI__.ipc.callHost(method, args); } };
    window.oreui.event = { on: function(name, cb){ return window.__DearOreUI__.events.on(name, cb); }, off: function(name, cb){ return window.__DearOreUI__.events.off(name, cb); } };
    window.oreui.diagnostic = { report: function(msg){ return window.__DearOreUI__.ipc.report(msg); } };
    window.DearOreUI = window.DearOreUI || {
        call: function(method, args) { return window.oreui.host.call(method, args); },
        report: function(msg) { return window.__DearOreUI__.ipc.report(msg); }
    };
    if (typeof console !== 'undefined' && console.log) {
        console.log('[DearOreUI] stage8 runtime injected, contextId=__DEAROREUI_CTX__, bridge=__DEAROREUI_BRIDGE__, facet=' + (hasFacet ? 'yes' : 'no') + '');
    }
    window.__DearOreUI__.silent = true;
})();