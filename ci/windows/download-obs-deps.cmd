if not exist %DepsBasePath% (
    curl -o %DepsBasePath%.zip -kLO https://obsproject.com/downloads/dependencies2017.zip -f --retry 5 -C -
    7z x %DepsBasePath%.zip -o%DepsBasePath%
) else (
    echo "OBS dependencies are already there. Download skipped."
)
