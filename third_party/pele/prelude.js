// Built-ins newer than Qt's JavaScript engine that Pele's render path uses.
var globalThis = this;
(function () {
  function define(object, name, value) {
    if (!object[name]) Object.defineProperty(object, name, { value: value, writable: true, configurable: true });
  }

  define(String.prototype, "trimStart", function () {
    return String(this).replace(/^\s+/, "");
  });
  define(String.prototype, "trimEnd", function () {
    return String(this).replace(/\s+$/, "");
  });
  define(String.prototype, "replaceAll", function (pattern, replacement) {
    if (pattern instanceof RegExp) {
      if (pattern.flags.indexOf("g") < 0) throw new TypeError("replaceAll needs a global RegExp");
      return String(this).replace(pattern, replacement);
    }
    var text = String(this);
    var needle = String(pattern);
    var parts = text.split(needle);
    if (typeof replacement !== "function") return parts.join(String(replacement));
    var out = parts[0];
    var at = parts[0].length;
    for (var i = 1; i < parts.length; i++) {
      out += replacement(needle, at, text) + parts[i];
      at += needle.length + parts[i].length;
    }
    return out;
  });
  define(String.prototype, "matchAll", function (pattern) {
    var flags = pattern.flags.indexOf("g") < 0 ? pattern.flags + "g" : pattern.flags;
    var re = new RegExp(pattern.source, flags);
    re.lastIndex = pattern.lastIndex || 0;
    var text = String(this);
    var found = [];
    var m;
    while ((m = re.exec(text)) !== null) {
      found.push(m);
      if (m[0] === "") re.lastIndex++;
    }
    return found[Symbol.iterator]();
  });
  define(Array.prototype, "flatMap", function (f, self) {
    var out = [];
    for (var i = 0; i < this.length; i++) {
      var v = f.call(self, this[i], i, this);
      if (Array.isArray(v)) for (var j = 0; j < v.length; j++) out.push(v[j]);
      else out.push(v);
    }
    return out;
  });
  define(Object, "hasOwn", function (o, k) {
    return Object.prototype.hasOwnProperty.call(o, k);
  });

  // Strings iterate by code point, so an emoji stays whole in for-of, spread and Array.from.
  if (Array.from("🎷").length !== 1) {
    Object.defineProperty(String.prototype, Symbol.iterator, {
      configurable: true,
      writable: true,
      value: function () {
        var text = String(this);
        var at = 0;
        var iterator = {
          next: function () {
            if (at >= text.length) return { value: undefined, done: true };
            var size = text.codePointAt(at) > 0xffff ? 2 : 1;
            var value = text.substr(at, size);
            at += size;
            return { value: value, done: false };
          },
        };
        iterator[Symbol.iterator] = function () {
          return iterator;
        };
        return iterator;
      },
    });
    var arrayFrom = Array.from;
    Array.from = function (items) {
      if (typeof items !== "string") return arrayFrom.apply(Array, arguments);
      var parts = [];
      for (var part of items) parts.push(part);
      var rest = Array.prototype.slice.call(arguments, 1);
      return arrayFrom.apply(Array, [parts].concat(rest));
    };
  }
})();
