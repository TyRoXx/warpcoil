var socket =
function () {
    var new_uri;
    if (location.protocol === "https:") {
        new_uri = "wss:";
    } else {
        new_uri = "ws:";
    }
    new_uri += "//" + location.host;
    return new WebSocket(new_uri);
}
();
socket.binaryType = "arraybuffer";
var client;
socket.onopen = function (event) {
    var pending_requests = {};
    var send_bytes = function (bytes) {
        socket.send(bytes);
    };
    var server = {
        display : function (message, on_result) {
            var log = document.getElementById("log");
            log.value += (message + "\n");
            log.scrollTop = log.scrollHeight;
            on_result([]);
        }
    };
    var receiver = make_receiver(pending_requests, server, send_bytes);
    client = make_client(pending_requests, send_bytes);
    socket.onmessage = function (event) {
        var view = new Uint8Array(event.data);
        for (var i = 0; i < view.length; ++i) {
            receiver(view[i]);
        }
    };
};
socket.onerror = function (event) {};
socket.onclose = function (event) {};
var submit = function () {
    if (client === undefined) {
        return;
    }
    client.publish(document.getElementById("input").value, function (error) {
        if (error) {
            alert(error.message);
        }
    });
};
