// The Web player's loader (linked as --pre-js): before main it fetches the data folder into the file system at /Data
// (assets.index, settings.asset and every file the index lists, the debug information next to C# assemblies when it is
// there) and takes the command line from the page's address: ?data=<folder URL>&args=<arguments separated by spaces>.
var Module = typeof Module !== 'undefined' ? Module : {};
(function () {
  var params = new URLSearchParams(typeof location !== 'undefined' ? location.search : '');
  var base = (params.get('data') || Module.helixDataUrl || 'Data').replace(/\/+$/, '');
  var extra = (params.get('args') || '').split(' ').filter(function (a) { return a.length > 0; });
  Module.arguments = (Module.arguments || []).concat(extra);

  function report(text) {
    (Module.printErr || console.error)(text);
  }

  Module.preRun = [].concat(Module.preRun || [], [function () {
    addRunDependency('helix-data');
    FS.mkdirTree('/Data');

    function fetchFile(path, optional) {
      return fetch(base + '/' + path).then(function (response) {
        if (!response.ok) {
          if (optional) {
            return null;
          }
          throw new Error(path + ': HTTP ' + response.status);
        }
        return response.arrayBuffer();
      }).then(function (buffer) {
        if (buffer) {
          var full = '/Data/' + path;
          FS.mkdirTree(full.substring(0, full.lastIndexOf('/')));
          FS.writeFile(full, new Uint8Array(buffer));
        }
        return buffer;
      });
    }

    Promise.all([fetchFile('settings.asset', true), fetchFile('assets.index', true)]).then(function (first) {
      if (!first[1]) {
        return [];
      }
      // One asset per line: 32 hex digits of the guid, a space, the path.
      var lines = new TextDecoder().decode(first[1]).split('\n');
      var loads = [];
      lines.forEach(function (line) {
        line = line.trim();
        if (line.length < 34 || line[0] === '#') {
          return;
        }
        var path = line.substring(33);
        loads.push(fetchFile(path, false));
        if (/\.dll$/i.test(path)) {
          loads.push(fetchFile(path.substring(0, path.length - 4) + '.pdb', true));
        }
      });
      return Promise.all(loads);
    }).then(function () {
      removeRunDependency('helix-data');
    }, function (error) {
      report('Helix: the data folder ' + base + ' could not be loaded: ' + error);
      removeRunDependency('helix-data');
    });
  }]);
})();
