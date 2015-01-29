# PowerShell script to install all dependencies
#

new-item lib -type directory
new-item lib\x86 -type directory
new-item lib\x86\Debug -type directory
new-item lib\include -type directory
new-item lib\include\glm -type directory
new-item build -type director

pushd

Start-FileDownload 'https://www.dropbox.com/s/46f40ixw758ntxm/trillek-win32-lib.zip?dl=0' -FileName win32lib.zip
7z -y x win32lib.zip

popd
