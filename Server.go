package main
import( "net"
    "os"
    "os/exec"
    "fmt"
    "bufio"
    "io/ioutil"
    "strings"
    "strconv"
    "encoding/json"
    "regexp"
)
type Config struct {
    MaxGames int `json:"Max_Games"`
    MaxGamesPerPlayer int `json:"Max_Games_Per_Player"`
    GameCommand string `json:"Game_Command"`
    MaxConnections int `json:"Max_Connections"`
}
type gameQuery struct {
    Id int
    Input chan chan string
}
type gameData struct {
    Id int
    Input chan string
}
type userData struct {
    Id int
    Socket net.Conn
}
//Below are regexen, They are used for parsing user input to send to the correct game
var joincmd *regexp.Regexp
var createcmd *regexp.Regexp
var sendcmd *regexp.Regexp
//the below channels are used for communication between the various goroutines
//the below 3 channels are used by the user id allocation server
var userRequests chan struct{}
var uidReplies chan int
var dcUserChannel chan int
//the below 3 channels are used by the game id allocation server
var gameRequests chan struct{}
var gidReplies chan int
var dcGameChannel chan int
//the below 4 channels are used by the server that maintains the mapping between games and game ids
var gamemapQuery chan gameQuery
var gamemapReply chan chan string
var gamemapAdd chan gameData
var gamemapRemove chan int
//the below channel is used to multiplex all the game's outputs for dispatch by the message router server
var gameoutput chan string
//the below channel is used to add user connections to the user connection tabke
var userconnadd chan userData
func main() {
    //first need to read config file
    if (len(os.Args)!=2) {
        fmt.Printf("Usage: %s [name of config file]",os.Args[0])
        os.Exit(1)
    }
    //initialize the regexen
    joincmd = regexp.MustCompile("^join (\\d+)")
    createcmd = regexp.MustCompile("^create (.+)")
    sendcmd = regexp.MustCompile("^send (\\d+) (.+)")
    filename := os.Args[1]
    config := readConfig(filename) //load configuration file details
    ln, err := net.Listen("tcp",":5000")
    if err != nil {
        //is a fatal error
    }
    userRequests = make(chan struct{})
    uidReplies = make(chan int)
    dcUserChannel = make(chan int)
    go uidServer(config.MaxConnections) //spin off the user id server
    gameRequests = make(chan struct{})
    gidReplies = make(chan int)
    dcGameChannel = make(chan int)
    go gidServer(config.MaxGames) //spin off the game id server
    gamemapQuery = make(chan gameQuery)
    gamemapReply = make(chan chan string)
    gamemapAdd = make(chan gameData)
    gamemapRemove = make(chan int)
    go gamemapServer() //spin off the game id mapping server
    gameoutput = make(chan string)
    userconnadd = make(chan userData)
    go msgrouter() //spin off the message routing server
    for {
        conn, err := ln.Accept()
        if err != nil {
            //handle error
        }
        userRequests<- struct{}{}
        uid := <-uidReplies
        if (uid != 0) { //server allocates real id
            var temp userData
            temp.Id = uid
            temp.Socket = conn
            userconnadd <- temp
            go handle(conn,config,uid,gameRequests,gidReplies) //spin off a goroutine to be associated with the server
        } else { //reject user
            conn.Write([]byte("Rejected!"))
            conn.Close()
        }
    }
}
func readConfig(filename string) Config { //used to read the config file(in JSON format)
    data,err := ioutil.ReadFile(filename)
    if err != nil {
        //fatal error
    }
    var config Config
    json.Unmarshal(data,&config)
    return config
}
func uidServer(maxUsers int) { //user id server
    idQueue := []int{}
    for i := 1; i <= maxUsers; i++ { //initialize set of available user ids
        idQueue = append(idQueue,i)
    }
    for {
        select {
            case <-userRequests: //a request has been made
                if (len(idQueue)!=0) {
                    uidReplies<- idQueue[0]
                    idQueue = idQueue[1:]
                } else {
                    uidReplies<- 0;
                }
            case id := <-dcUserChannel: //a user has disconnected
                idQueue = append(idQueue,id)
        }
    }
}
func game(gid int, input <-chan string, gameCmd string,cmdargs []string) { //associated with an individual game
    spawner:= exec.Command(gameCmd,cmdargs...)
    gameIn,err := spawner.StdinPipe()
    if err != nil {
    }
    gameOut,err := spawner.StdoutPipe()
    spawner.Start() //create game as subprocess
    idStr := strconv.Itoa(gid)
    linechan := make(chan string,10)
    go func() { //used to read lines from the output of the subprocess
        lineReader := bufio.NewScanner(gameOut)
        for lineReader.Scan() {
            line := lineReader.Text()
            linechan <- line
        }
    }()
    for {
        select {
            case temp:= <-input: //input is to be sent to the game
                gameIn.Write([]byte(temp))
            case line:= <-linechan: //output is to be routed away from the game
                gameoutput<- (idStr+" "+line);
        }
    }
}
func msgrouter() { //used to route game output to the correct user
    usersocket := make(map[int]net.Conn) //maintains map of users to sockets
    for {
        select {
            case temp:= <-userconnadd: //instruction to add a user connection
                usersocket[temp.Id]=temp.Socket
            case temp:= <-gameoutput: //an output message from a game needs to be demultiplexed to the correct user
                parts := strings.Fields(temp)
                uid,err := strconv.Atoi(parts[1])
                if (err!=nil) {
                }
                newmsg := strings.Join(append(parts[0:1],parts[2:]...)," ")
                usersocket[uid].Write([]byte(newmsg))
        }
    }
}
func gidServer(maxGames int) { //game id allocation server
    idQueue := []int{}
    for i := 1; i <= maxGames; i++ {
        idQueue = append(idQueue,i)
    }
    for {
        select {
        case <-gameRequests: //request to create a new game
                if (len(idQueue)!=0) { //accept request
                    gidReplies<- idQueue[0]
                    idQueue = idQueue[1:]
                } else { //deny request
                    gidReplies<- 0
                }
            case id := <-dcGameChannel: //notification that a game has ended
                idQueue = append(idQueue,id)
        }
    }
}
func gamemapServer() { //maintains mapping between game ids and game input channel
    games := make(map[int]chan string)
    for {
        select {
        case temp := <-gamemapQuery:
                temp.Input <- games[temp.Id]
            case temp := <-gamemapAdd:
                games[temp.Id]=temp.Input
            case temp := <-gamemapRemove:
                delete(games,temp)
        }
    }
}
func handle(connect net.Conn,config Config, uid int,gameRequests chan struct{},gidReplies chan int) { //handler for an individual user
    games :=make(map[int]chan string)
    input := bufio.NewReader(connect)
    connect.Write([]byte("ACCEPTED " + strconv.Itoa(uid)+"\n"))
    line,err := input.ReadString('\n')
    for err == nil {
        fmt.Println(line)
        if matches := joincmd.FindStringSubmatch(line); matches != nil { //user has made a request to join a game
            fmt.Println(line)
            gid, err := strconv.Atoi(matches[1])
            if err != nil {
            }
            var query gameQuery
            query.Id = gid
            query.Input = make(chan chan string)
            gamemapQuery <- query
            games[gid] = <-query.Input
            joinCmd := "0 join " + strconv.Itoa(uid) + "\r\n"
            fmt.Println(joinCmd)
            games[gid] <- joinCmd
        } else if matches := createcmd.FindStringSubmatch(line); matches != nil { //user has made a request to create a game
            gameRequests <- struct{}{}
            cmdargs := strings.Fields(matches[1])
            gid := <-gidReplies
            if (gid!=0) {
                input := make(chan string,10)
                var data gameData
                data.Id = gid
                data.Input = input
                gamemapAdd <- data
                games[gid]=data.Input
                gameCmdSet := strings.Fields(config.GameCommand)
                cmdargs = append(gameCmdSet[1:],cmdargs...)
                gameCmd := gameCmdSet[0]
                connect.Write([]byte("ACCEPTED" + strconv.Itoa(gid)+"\n"))
                go game(gid,games[gid],gameCmd,cmdargs)
                joinCmd := "0 join " + strconv.Itoa(uid) + "\r\n"
                games[gid] <- joinCmd
            } else { //game proposal rejected
                connect.Write([]byte("REJECTED"));
            }
        } else if matches := sendcmd.FindStringSubmatch(line); matches != nil { //user wants to send a message to a game
            gid,err := strconv.Atoi(matches[1]);
            msg := matches[2]
            if err != nil {
            }
            games[gid] <- (strconv.Itoa(uid)+" "+msg+"\n")
        } else {
            fmt.Println([]byte(line))
        }
        line,err = input.ReadString('\n')
    }
}
