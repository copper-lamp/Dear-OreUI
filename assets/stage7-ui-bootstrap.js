(function(){
    window.__DearOreUI__ = window.__DearOreUI__ || {};
    window.__DearOreUI__.ui = window.__DearOreUI__.ui || {};
    window.__DearOreUI__.ui.executed = true;
    window.__DearOreUI__.ui.contextId = "__DEAROREUI_CTX__";
    window.__DearOreUI__.ui.specs = [];
    window.__DearOreUI__.ui.debug = [];
    window.__DearOreUI__.ui.dbg = function(msg) {
        window.__DearOreUI__.ui.debug.push(msg);
        try { if (window.__DearOreUI__ && window.__DearOreUI__.ipc) window.__DearOreUI__.ipc.report('dbg:' + msg); } catch (e) {}
    };
    function dearOreUiBuildDom(parent, nodes) {
        var last = null;
        for (var i = 0; i < nodes.length; i++) {
            var n = nodes[i];
            if (n.t && n.t.toLowerCase() === 'script') { continue; }
            var el = document.createElement(n.t || 'div');
            if (n.s) {
                try { el.style.cssText = n.s; } catch (e) { window.__DearOreUI__.ui.dbg('style_err:' + (e && e.message)); }
            }
            if (n.a) {
                for (var j = 0; j < n.a.length; j++) {
                    try { el.setAttribute(n.a[j][0], n.a[j][1]); } catch (e) {}
                }
            }
            if (n.x) {
                el.textContent = n.x;
            }
            if (n.c && n.c.length) {
                dearOreUiBuildDom(el, n.c);
            }
            parent.appendChild(el);
            last = el;
            if (n.st) {
                el.__dearOreUiStates = {};
                el.__dearOreUiBase = n.b || '';
                for (var k = 0; k < n.st.length; k++) {
                    el.__dearOreUiStates[n.st[k][0]] = n.st[k][1];
                }
                try { dearOreUiWireState(el); } catch (e) {}
            }
        }
        return last;
    }
    function dearOreUiSetState(el, state) {
        if (!el || !el.__dearOreUiStates) return;
        if (el.__dearOreUiDisabled) return;
        var css = el.__dearOreUiStates[state];
        if (css === undefined) css = el.__dearOreUiStates['default'];
        if (css === undefined) return;
        try { el.style.cssText = (el.__dearOreUiBase || '') + css; } catch (e) {}
        el.__dearOreUiState = state;
    }
    function dearOreUiOn(el, type, fn) {
        try { el.addEventListener(type, fn); return true; } catch (e) { return false; }
    }
    function dearOreUiWireState(el) {
        if (!el || !el.__dearOreUiStates) return;
        el.__dearOreUiDisabled = (el.getAttribute('aria-disabled') === 'true');
        if (!dearOreUiOn(el, 'mouseenter', function() {
            if (el.__dearOreUiDisabled) return;
            dearOreUiSetState(el, 'hovered');
        })) {
            dearOreUiOn(el, 'mouseover', function() {
                if (el.__dearOreUiDisabled) return;
                dearOreUiSetState(el, 'hovered');
            });
        }
        if (!dearOreUiOn(el, 'mouseleave', function() {
            if (el.__dearOreUiDisabled) return;
            dearOreUiSetState(el, el.__dearOreUiFocused ? 'focused' : 'default');
        })) {
            dearOreUiOn(el, 'mouseout', function() {
                if (el.__dearOreUiDisabled) return;
                dearOreUiSetState(el, el.__dearOreUiFocused ? 'focused' : 'default');
            });
        }
        dearOreUiOn(el, 'mousedown', function() {
            if (el.__dearOreUiDisabled) return;
            dearOreUiSetState(el, el.__dearOreUiFocused ? 'pressedFocused' : 'pressed');
        });
        dearOreUiOn(el, 'mouseup', function() {
            if (el.__dearOreUiDisabled) return;
            dearOreUiSetState(el, el.__dearOreUiFocused ? 'focused' : 'hovered');
        });
        dearOreUiOn(el, 'focus', function() {
            el.__dearOreUiFocused = true;
            if (el.__dearOreUiDisabled) return;
            dearOreUiSetState(el, 'focused');
        });
        dearOreUiOn(el, 'blur', function() {
            el.__dearOreUiFocused = false;
            if (el.__dearOreUiDisabled) return;
            dearOreUiSetState(el, 'default');
        });
    }
    window.__DearOreUI__.ui.setState = function(elOrId, state) {
        var el = (typeof elOrId === 'string') ? document.getElementById(elOrId) : elOrId;
        if (el) dearOreUiSetState(el, state);
    };
    window.__DearOreUI__.ui.getState = function(elOrId) {
        var el = (typeof elOrId === 'string') ? document.getElementById(elOrId) : elOrId;
        return el ? (el.__dearOreUiState || 'default') : null;
    };
    window.__DearOreUI__.ui.mount = function(spec) {
        var container = document.getElementById(spec.containerId);
        if (!container) {
            container = document.createElement('div');
            container.id = spec.containerId;
            try {
                container.style.cssText =
                    'position:fixed;top:0;left:0;right:0;bottom:0;' +
                    'z-index:2147483647;' +
                    'pointer-events:' + (spec.pointerEvents ? 'auto' : 'none') + ';';
            } catch (e) {}
            var parent = document.body || document.documentElement;
            if (parent) {
                parent.appendChild(container);
            } else {
                throw new Error('no document.body or documentElement');
            }
        }
        container.innerHTML = '';
        try {
            container.__dearOreUiRoot = dearOreUiBuildDom(container, spec.body);
            window.__DearOreUI__.ui.dbg('body_built:' + spec.containerId);
        } catch (e) {
            window.__DearOreUI__.ui.dbg('body_build_err:' + (e && e.message));
            window.__DearOreUI__.ui.report('mount_error:' + (e && e.message));
        }
        for (var i = 0; i < spec.styles.length; i++) {
            var link = document.createElement('link');
            link.rel = 'stylesheet';
            link.href = spec.styles[i];
            if (document.head) document.head.appendChild(link);
        }
        for (var i = 0; i < spec.scripts.length; i++) {
            var script = document.createElement('script');
            script.src = spec.scripts[i];
            script.async = true;
            if (document.body) document.body.appendChild(script);
        }
    };
    window.__DearOreUI__.ui.appendTo = function(containerId, nodes) {
        var container = document.getElementById(containerId);
        if (!container) return false;
        var target = container.__dearOreUiRoot || container.firstElementChild || container;
        try { dearOreUiBuildDom(target, nodes); } catch (e) { return false; }
        container.__dearOreUiAppended = (container.__dearOreUiAppended || 0) + 1;
        return true;
    };
    window.__DearOreUI__.ui.unmount = function(containerId) {
        var container = document.getElementById(containerId);
        if (container && container.parentNode) {
            container.parentNode.removeChild(container);
        }
    };
    window.__DearOreUI__.ui.report = function(msg) {
        try { if (window.__DearOreUI__ && window.__DearOreUI__.ipc) window.__DearOreUI__.ipc.report(msg); } catch (e) {}
    };
    window.__DearOreUI__.ui.mountAll = function() {
        for (var i = 0; i < window.__DearOreUI__.ui.specs.length; i++) {
            var spec = window.__DearOreUI__.ui.specs[i];
            try {
                window.__DearOreUI__.ui.mount(spec);
                window.__DearOreUI__.ui.report('mounted:' + spec.containerId);
                if (window.console && console.log) console.log('[DearOreUI] mounted ' + spec.containerId);
            } catch (e) {
                window.__DearOreUI__.ui.report('mount_error:' + spec.containerId + ':' + (e && e.message));
                if (window.console && console.error) console.error('[DearOreUI] mount failed: ' + (e && e.message));
            }
        }
    };
    function dearOreUiReadyMount() {
        if (document.body) {
            window.__DearOreUI__.ui.mountAll();
            window.__DearOreUI__.ui.mounted = true;
            window.__DearOreUI__.ui.report('mount_all_done');
            return true;
        }
        return false;
    }
    if (!dearOreUiReadyMount()) {
        var dearOreUiIntervalId = setInterval(function() {
            if (dearOreUiReadyMount()) {
                clearInterval(dearOreUiIntervalId);
            }
        }, 100);
        document.addEventListener('DOMContentLoaded', function() {
            if (dearOreUiReadyMount()) {
                clearInterval(dearOreUiIntervalId);
            }
        });
        setTimeout(function() { clearInterval(dearOreUiIntervalId); }, 10000);
    }
})();