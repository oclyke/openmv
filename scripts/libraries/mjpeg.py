# This file is part of the OpenMV project.
#
# Copyright (c) 2023 Ibrahim Abdelkader <iabdalkader@openmv.io>
# Copyright (c) 2023 Kwabena W. Agyeman <kwagyeman@openmv.io>
#
# This work is licensed under the MIT license, see the file LICENSE for details.
#
# This is an extension to the mjpeg C user-module.

from umjpeg import *
import errno
import re
import socket


class mjpeg_server:
    def __valid_tcp_socket(self):  # private
        if self.__tcp__socket is None:
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                try:
                    if hasattr(socket, "SO_REUSEADDR"):
                        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                    s.bind(self.__myaddr)
                    s.listen(0)
                    s.settimeout(5)
                    self.__tcp__socket, self.__client_addr = s.accept()
                except OSError:
                    pass
                s.close()
            except OSError:
                self.__tcp__socket = None
        return self.__tcp__socket is not None

    def __close_tcp_socket(self):  # private
        if self.__tcp__socket is not None:
            self.__tcp__socket.close()
            self.__tcp__socket = None
        if self.__playing:
            self.__playing = False
            if self.__teardown_cb:
                self.__teardown_cb(self.__pathname)

    def __init__(self, network_if, port=8080):  # private
        self.__network = network_if
        self.__myip = self.__network.ifconfig()[0]
        self.__myaddr = (self.__myip, port)
        self.__tcp__socket = None
        self.__setup_cb = None
        self.__teardown_cb = None
        self.__pathname = ""
        self.__playing = False
        self.__boundary = "OpenMVCamMJPEG"
        print("IP Address:Port %s:%d\nRunning..." % self.__myaddr)

    def register_setup_cb(self, cb):  # public
        self.__setup_cb = cb

    def register_teardown_cb(self, cb):  # public
        self.__teardown_cb = cb

    def __send_http_response(self, code, name, extra=""):  # private
        self.__tcp__socket.send("HTTP/1.0 %d %s\r\n%s\r\n" % (code, name, extra))

    def __send_http_response_ok(self, extra=""):  # private
        self.__send_http_response(200, "OK", extra)

    def __parse_mjpeg_request(self, data):  # private
        s = data.decode().splitlines()
        if s and len(s) >= 1:
            line0 = s[0].split(" ")
            request = line0[0]
            self.__pathname = re.sub("(http://[a-zA-Z0-9\-\.]+(:\d+)?(/)?)", "/", line0[1])
            if self.__pathname != "/" and self.__pathname.endswith("/"):
                self.__pathname = self.__pathname[:-1]
            if line0[2] == "HTTP/1.0" or line0[2] == "HTTP/1.1":
                if request == "GET":
                    self.__playing = True
                    self.__send_http_response_ok(
                        "Server: OpenMV Cam\r\n"
                        "Content-Type: multipart/x-mixed-replace;boundary=" + self.__boundary + "\r\n"
                        "Connection: close\r\n"
                        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                        "Pragma: no-cache\r\n"
                        "Expires: 0\r\n"
                    )
                    if self.__setup_cb:
                        self.__setup_cb(self.__pathname)
                    return
                else:
                    self.__send_http_response(501, "Not Implemented")
                    return
        self.__send_http_response(400, "Bad Request")

    def __send_mjpeg(self, image_callback, quality):  # private
        try:
            self.__tcp__socket.settimeout(5)
            img = image_callback(self.__pathname).to_jpeg(quality=quality)
            mjpeg_header = (
                "\r\n--" + self.__boundary + "\r\n"
                "Content-Type: image/jpeg\r\n"
                "Content-Length:" + str(img.size()) + "\r\n\r\n"
            )
            self.__tcp__socket.sendall(mjpeg_header)
            self.__tcp__socket.sendall(img)
        except OSError:
            self.__close_tcp_socket()

    def stream(self, image_callback, quality=90):  # public
        while True:
            if self.__valid_tcp_socket():
                try:
                    self.__tcp__socket.settimeout(0.01)
                    try:
                        data = self.__tcp__socket.recv(1400)
                        if data and len(data):
                            self.__parse_mjpeg_request(data)
                        else:
                            raise OSError
                    except OSError as e:
                        if e.errno != errno.EAGAIN and e.errno != errno.ETIMEDOUT:
                            raise e
                    if self.__playing:
                        self.__send_mjpeg(image_callback, quality)
                except OSError:
                    self.__close_tcp_socket()
