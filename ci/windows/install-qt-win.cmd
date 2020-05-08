if not exist %QtBaseDir% (
	curl -kLO https://cdn-fastly.obsproject.com/downloads/Qt_5.10.1.7z -f --retry 5 -z Qt_5.10.1.7z
    7z x Qt_5.10.1.7z -o%QtBaseDir%
) else (
	echo "Qt is already installed. Download skipped."
)

dir %QtBaseDir%
