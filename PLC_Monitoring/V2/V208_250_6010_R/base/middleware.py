import time

from user_agents import parse

from .models import VisitorLog


IGNORE_PATHS = [

    "/admin/",
    "/admin/jsi18n/",
    "/static/",
    "/media/",
    "/favicon.ico",
    "/robots.txt",
    "/api/version/",
    "/api/alert-configs/all/",
    "/api/plc-keys/",
    "/api/settings/"
]


IGNORE_PREFIX = [

    "/api/settings/stream/",
    "/api/logs/",

]


def should_ignore_request(
    request,
):

    path = request.path

    user_agent = (
        request.META.get(
            "HTTP_USER_AGENT",
            ""
        ).lower()
    )

    ip = get_client_ip(
        request
    )

    ignored_paths = [

        "/admin/",
        "/admin/jsi18n/",

        "/api/settings/stream/",

        "/favicon.ico",

        "/static/",

    ]

    if any(
        path.startswith(i)
        for i
        in ignored_paths
    ):
        return True

    if (
        "python-requests"
        in user_agent
    ):
        return True

    if ip in [

        "127.0.0.1",

        "localhost",

        "192.168.2.22",

    ]:
        return True

    return False


def get_client_ip(request):

    forwarded = request.META.get(
        "HTTP_X_FORWARDED_FOR"
    )

    if forwarded:

        return (
            forwarded
            .split(",")[0]
            .strip()
        )

    real_ip = request.META.get(
        "HTTP_X_REAL_IP"
    )

    if real_ip:
        return real_ip

    return request.META.get(
        "REMOTE_ADDR"
    )

class VisitorLogger:

    def __init__(self, get_response):
        self.get_response = get_response

    def should_log(
        self,
        request,
        response,
    ):

        path = request.path

        if any(
            path.startswith(i)
            for i in IGNORE_PATHS
        ):
            return False

        if any(
            path.startswith(i)
            for i in IGNORE_PREFIX
        ):
            return False

        # فقط GET/POST
        if request.method not in [
            "GET",
            "POST",
        ]:
            return False

        # درخواست ajax
        if (
            request.headers.get(
                "X-Requested-With"
            )
            ==
            "XMLHttpRequest"
        ):
            return False

        # فایل استاتیک
        if (
            "." in path
            and path.split(".")[-1]
            in [
                "css",
                "js",
                "png",
                "jpg",
            ]
        ):
            return False

        return True

    def __call__(self, request):

        start = time.time()

        response = self.get_response(
            request
        )

        if not self.should_log(
            request,
            response,
        ):
            return response

        try:

            ua = parse(
                request.META.get(
                    "HTTP_USER_AGENT",
                    ""
                )
            )
            if should_ignore_request(request):
                return response
            VisitorLog.objects.create(

                ip=get_client_ip(
                    request
                ),

                path=request.get_full_path(),

                method=request.method,

                status=response.status_code,

                duration_ms=int(
                    (
                        time.time()
                        -
                        start
                    )
                    *
                    1000
                ),

                referer=request.META.get(
                    "HTTP_REFERER",
                    ""
                ),

                user_agent=request.META.get(
                    "HTTP_USER_AGENT",
                    ""
                ),

                browser=ua.browser.family,

                os=ua.os.family,

                device=ua.device.family,

                is_mobile=ua.is_mobile,

                is_bot=ua.is_bot,

            )

        except Exception:
            pass

        return response