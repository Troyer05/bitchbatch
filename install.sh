sudo apt update
sudo apt install gcc -y

g++ *.cpp -Iheader -o biba

sudo cp ./biba /usr/sbin

clear

echo "Bitch Batch installiert - sudo biba"
