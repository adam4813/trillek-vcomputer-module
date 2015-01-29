# PowerShell script to install all dependencies
#

new-item lib -type directory
new-item lib\x86 -type directory
new-item lib\x86\Debug -type directory
new-item lib\include -type directory
new-item lib\include\glm -type directory
new-item build -type director

pushd

Start-FileDownload 'https://mega.co.nz/#!slEAQBoQ!0ROIHTTCUBUVNvn3Kjrp5E7VyBv1yUeQlK6vk3MgGo0'
7z -y x trillek-win32-lib.zip

popd
