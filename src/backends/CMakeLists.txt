target_sources(QArchiver PRIVATE
    ArchiveBackend.cpp
    ArchiveBackend.h

    tar/TarBackend.cpp
    tar/TarBackend.h
    tar/TarOutputParser.cpp
    tar/TarOutputParser.h

    zip/ZipBackend.h

    sevenzip/SevenZipBackend.cpp
    sevenzip/SevenZipBackend.h

    rar/RarBackend.cpp
    rar/RarBackend.h

    stream/StreamBackend.cpp
    stream/StreamBackend.h
)

# Read-only libarchive backend (zip/tar/7z/streams without an external process).
if(QARCHIVER_HAS_LIBARCHIVE)
    target_sources(QArchiver PRIVATE
        archive/LibArchiveBackend.cpp
        archive/LibArchiveBackend.h
    )
endif()
