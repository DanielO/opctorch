net = require('net');

var OPCTorch = function(port) {
  this.port = port;
}

OPCTorch.prototype.cmd = function(s) {
  con = net.connect(this.port);
  con.write(s + "\n");
};

OPCTorch.prototype.set = function(name, val) {
  this.cmd("set " + name + " " + val);
};

OPCTorch.prototype.version = function(fn) {
  var v = "1.0";
  fn(v);
};

OPCTorch.prototype.serialNumber = function(fn) {
    fn("000001");
};

OPCTorch.prototype.brightness = function(b) {
  this.set("brightness", b);
};

module.exports = OPCTorch;
