//summarize MSVC errors from an appveyor log
// compile with 'go build summarize-appveyor-log.go'
// takes 0 or 1 args; with 0, gets log from latest
// build. with 1, uses that file as raw json-like log
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"regexp"
	"sort"
	"strings"
)

const (
	headerKey = "Authorization"
	headerVal = "Bearer %s"
	projUrl   = "https://ci.appveyor.com/api/projects/mpictor/stepcode"
	//"https://ci.appveyor.com/api/buildjobs/2rjxdv1rnb8jcg8y/log"
	logUrl     = "https://ci.appveyor.com/api/buildjobs/%s/log"
	consoleUrl = "https://ci.appveyor.com/api/buildjobs/%s/console"
)

func main() {
	var rawlog io.ReadCloser
	var build string
	var err error
	if len(os.Args) == 2 {
		rawlog, build, err = processArgv()
	} else {
		rawlog, build, err = getLog()
	}
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: %s\n", err)
		return
	}
	defer rawlog.Close()
	log := decodeConsole(rawlog)
	warns, errs := countMessages(log)
	fi, err := os.Create(fmt.Sprintf("appveyor-%s.smy", build))
	if err != nil {
		fmt.Fprintf(os.Stderr, "ERROR: %s\n", err)
		return
	}
	printMessages("error", errs, fi)
	printMessages("warning", warns, fi)

	fmt.Printf("done\n")

}

/* categorizes warnings and errors based upon the MSVC message number (i.e. C4244)
 * the regex will match lines like
c:\projects\stepcode\src\base\sc_benchmark.h(45): warning C4251: 'benchmark::descr' : class 'std::basic_string<char,std::char_traits<char>,std::allocator<char>>' needs to have dll-interface to be used by clients of class 'benchmark' [C:\projects\STEPcode\build\src\base\base.vcxproj]
[00:03:48] C:\projects\STEPcode\src\base\sc_benchmark.cc(61): warning C4244: '=' : conversion from 'SIZE_T' to 'long', possible loss of data [C:\projects\STEPcode\build\src\base\base.vcxproj]*
*/
func countMessages(log []string) (warns, errs map[string][]string) {
	warns = make(map[string][]string)
	errs = make(map[string][]string)
	fname := " *(.*)"            // $1
	fline := `(?:\((\d+)\)| ): ` // $2 - either line number in parenthesis or a space, followed by a colon
	msgNr := `([A-Z]+\d+): `     // $3 - C4251, LNK2005, etc
	msgTxt := `([^\[]*) `        // $4
	tail := `\[[^\[\]]*\]`
	warnRe := regexp.MustCompile(fname + fline + `warning ` + msgNr + msgTxt + tail)
	errRe := regexp.MustCompile(fname + fline + `(?:fatal )?error ` + msgNr + msgTxt + tail)
	for _, line := range log {
		if warnRe.MatchString(line) {
			key := warnRe.ReplaceAllString(line, "$3")
			path := strings.ToLower(warnRe.ReplaceAllString(line, "$1:$2"))
			arr := warns[key]
			if arr == nil {
				arr = make([]string, 5)
				//detailed text as first string in array
				text := warnRe.ReplaceAllString(line, "$4")
				arr[0] = fmt.Sprintf("%s", text)
			}
			//eliminate duplicates
			match := false
			for _, l := range arr {
				if l == path {
					match = true
				}
			}
			if !match {
				warns[key] = append(arr, path)
			}
		} else if errRe.MatchString(line) {
			key := errRe.ReplaceAllString(line, "$3")
			path := strings.ToLower(errRe.ReplaceAllString(line, "$1:$2"))
			arr := errs[key]
			if arr == nil {
				arr = make([]string, 5)
				//detailed text as first string in array
				text := errRe.ReplaceAllString(line, "$4")
				arr[0] = fmt.Sprintf("%s", text)
			}
			//eliminate duplicates
			match := false
			for _, l := range arr {
				if l == path {
					match = true
				}
			}
			if !match {
				errs[key] = append(arr, path)
			}
		}
	}
	return
}

func printMessages(typ string, m map[string][]string, w io.Writer) {
	//sort keys
	keys := make([]string, 0, len(m))
	for key := range m {
		keys = append(keys, key)
	}
	sort.Strings(keys)
	for _, k := range keys {
		for i, l := range m[k] {
			//first string is an example,  not a location
			if i == 0 {
				fmt.Fprintf(w, "%s %s (i.e. \"%s\")\n", typ, k, l)
			} else if len(l) > 1 { //not sure where blank lines are coming from...
				fmt.Fprintf(w, "  >> %s\n", l)
			}
		}
	}
}

//structs from http://json2struct.mervine.net/

//{"values":[{"i":0,"t":"Specify a project or solution file. The directory does not contain a project or solution file.\r\n","dt":"00:00:04","bg":12,"fg":15}]}
type AppVeyorConsoleLines struct {
	Values []struct {
		I        int    `json:"i"`
		Text     string `json:"t"`
		DateTime string `json:"dt"`
		BgColor  int    `json:"bg"`
		FgColor  int    `json:"fg"`
	}
}
type AppVeyorBuild struct {
	Build struct {
		/*BuildNumber int `json:"buildNumber"`*/
		Version string `json:"version"`
		Jobs    []struct {
			JobID string `json:"jobId"`
		} `json:"jobs"`
	} `json:"build"`
}

func splitAppend(log *[]string, blob string) {
	//blob = strings.Replace(blob,"\r\n", "\n",-1)
	blob = strings.Replace(blob, "\\", "/", -1)
	r := strings.NewReader(blob)
	unwrapScanner := bufio.NewScanner(r)
	for unwrapScanner.Scan() {
		txt := unwrapScanner.Text()
		//fmt.Printf("%s\n", txt)
		*log = append(*log, txt)
	}
}

//calculate length of string without escape chars
// func escapeLen(s string)(l int) {
// 	//s = strings.Replace(s,"\\\\", "/",-1)
// 	s = strings.Replace(s,"\\\"", "",-1)
// 	s = strings.Replace(s,"\r\n", "RN",-1)
// 	return len(s)
// }


//decode the almost-JSON console data from appveyor
func decodeConsole(r io.Reader) (log []string) {
	wrapper := Wrap(r)
	dec := json.NewDecoder(wrapper)
	var consoleLines AppVeyorConsoleLines
	var err error
	var txtBlob string
	err = dec.Decode(&consoleLines)
	if err == io.EOF {
		err = nil
	}
	if err == nil {
		for _, l := range consoleLines.Values {
			txtBlob += l.Text
			//el := escapeLen(l.Text)
			//something inserts newlines at 229 chars (+\n\r == 231) (found in CMake output)
			lenTwoThreeOne := len(l.Text) == 231
			if lenTwoThreeOne {
				txtBlob = strings.TrimSuffix(txtBlob, "\r\n")
			}
			//something else starts new log lines at 1024 chars without inserting newlines (found in CTest error output)
			if len(l.Text) != 1024 && !lenTwoThreeOne {
				//fmt.Printf("sa for l %d, el %d\n", len(l.Text),el)
				splitAppend(&log, txtBlob)
				txtBlob = ""
			}
		}
	} else {
		fmt.Printf("decode err %s\n", err)
	}
	if len(txtBlob) > 0 {
		splitAppend(&log, txtBlob)
	}
	return
}

func processArgv() (log io.ReadCloser, build string, err error) {
	fname := os.Args[1]
	if len(fname) < 14 {
		err = fmt.Errorf("Name arg '%s' too short. Run as '%s appveyor-NNN.log'", fname, os.Args[0])
		return
	}
	buildRe := regexp.MustCompile(`appveyor-(.+).log`)
	build = buildRe.ReplaceAllString(fname, "$1")
	if len(build) == 0 {
		err = fmt.Errorf("No build id in %s", fname)
		return
	}
	log, err = os.Open(fname)
	return
}

func getLog() (log io.ReadCloser, build string, err error) {
	client := &http.Client{}
	req, err := http.NewRequest("GET", projUrl, nil)
	if err != nil {
		return
	}
	apikey := os.Getenv("APPVEYOR_API_KEY")
	//api key isn't necessary for read-only queries on public projects
	if len(apikey) > 0 {
		req.Header.Add(headerKey, fmt.Sprintf(headerVal, apikey))
	} //else {
	//	fmt.Printf("Env var APPVEYOR_API_KEY is not set.")
	//}
	resp, err := client.Do(req)
	if err != nil {
		return
	}

	build, job := decodeProjInfo(resp.Body)
	fmt.Printf("build #%s, jobId %s\n", build, job)
	resp, err = http.Get(fmt.Sprintf(consoleUrl, job))
	if err != nil {
		return
	}
	logName := fmt.Sprintf("appveyor-%s.log", build)
	fi, err := os.Create(logName)
	if err != nil {
		return
	}
	_, err = io.Copy(fi, resp.Body)
	if err != nil {
		return
	}
	log, err = os.Open(logName)
	if err != nil {
		log = nil
	}
	return
}

func decodeProjInfo(r io.Reader) (vers string, job string) {
	dec := json.NewDecoder(r)
	var av AppVeyorBuild
	err := dec.Decode(&av)
	if err != io.EOF && err != nil {
		fmt.Printf("err %s\n", err)
		return
	}
	if len(av.Build.Jobs) != 1 {
		return
	}
	vers = av.Build.Version
	job = av.Build.Jobs[0].JobID
	return
}

//wrap a reader, modifying content to make the json decoder happy
//only tested with data from appveyor console
type jsonWrapper struct {
	source io.Reader
	begin  bool
	end    bool
}

func Wrap(r io.Reader) *jsonWrapper {
	return &jsonWrapper{
		source: r,
		begin:  true,
	}
}

// func nonNeg(n int) (int) {
// 	if n < 0 {
// 		return 0
// 	}
// 	return n
// }

func (w *jsonWrapper) Read(p []byte) (n int, err error) {
	if w.end {
		return 0, io.EOF
	}
	if w.begin {
		w.begin = false
		n = copy(p, []byte(`{"values":[`))
	}
	m, err := w.source.Read(p[n:])
	n += m
	if err == io.EOF {
		w.end = true
		if n < len(p) {
			n = copy(p, []byte(`{"dummy":"data"}]}`))
		} else {
			err = fmt.Errorf("No room to terminate JSON struct with '}'\n")
		}
	}
	return
}

// kate: indent-width 8; space-indent off; replace-tabs off; replace-tabs-save off; replace-trailing-space-save on; remove-trailing-space on; tab-intent on; tab-width 8; show-tabs off;
