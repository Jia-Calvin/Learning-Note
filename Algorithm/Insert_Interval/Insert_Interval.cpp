#include <algorithm>
#include <iostream>
#include <vector>
using namespace std;
struct Interval {
    int start;
    int end;
    Interval() : start(0), end(0) {}
    Interval(int s, int e) : start(s), end(e) {}
};

vector<int> get_pos(vector<Interval>& intervals, int cut) {
    vector<int> pos(2);
    for (int i = 0; i < intervals.size(); i++) {
        int currentStart = intervals[i].start;
        int currentEnd = intervals[i].end;
        if (cut - currentStart >= 0 && cut - currentEnd <= 0) {
            pos[0] = 0;
            pos[1] = i;
            return pos;
        } else if (cut - currentStart < 0) {
            pos[0] = 1;
            pos[1] = i;
            return pos;
        }
    }
    pos[0] = 1;
    pos[1] = intervals.size() - 1;
    return pos;
}

vector<Interval> insert(vector<Interval>& intervals, Interval newInterval) {
    if (newInterval.start > newInterval.end) {
        return intervals;
    }

    if (intervals.size() == 0) {
        intervals.push_back(newInterval);
        return intervals;
    }

    vector<int> start_pos = get_pos(intervals, newInterval.start);
    vector<int> end_pos = get_pos(intervals, newInterval.end);

    if (start_pos[0] == 0) {
        newInterval.start = intervals[start_pos[1]].start;
    }
    if (end_pos[0] == 0) {
        newInterval.end = intervals[end_pos[1]].end;
    }

    if (start_pos[1] == end_pos[1] && end_pos[0] == start_pos[0]) {
        if (newInterval.end < intervals[start_pos[1]].start) {
            intervals.insert(intervals.begin() + start_pos[1], newInterval);
            return intervals;
        } else if (newInterval.start > intervals[start_pos[1]].end) {
            intervals.insert(intervals.begin() + start_pos[1] + 1, newInterval);
            return intervals;
        }
    }

    int offset = end_pos[0] == 1 ? newInterval.end > intervals[end_pos[1]].end ? 1 : 0 : 1;

    intervals[start_pos[1]].start = newInterval.start;
    intervals[start_pos[1]].end = newInterval.end;

    if (start_pos[1] + 1 < end_pos[1] + offset) {
        intervals.erase(intervals.begin() + start_pos[1] + 1,
                        intervals.begin() + end_pos[1] + offset);
    }
    return intervals;
}

int main(int argc, char const* argv[]) {
    vector<Interval> intervals;
    Interval newInterval(1, 5);
    intervals.push_back(newInterval);
    newInterval.start = 7;
    newInterval.end = 8;
    intervals.push_back(newInterval);
    // newInterval.start = 7;
    // newInterval.end = 8;
    // intervals.push_back(newInterval);
    // newInterval.start = 10;
    // newInterval.end = 11;
    // intervals.push_back(newInterval);
    // newInterval.start = 13;
    // newInterval.end = 16;
    // intervals.push_back(newInterval);

    newInterval.start = 9;
    newInterval.end = 11;
    intervals = insert(intervals, newInterval);

    // vector<int> pos = get_pos(intervals, 0);
    // cout << pos[0] << ", " << pos[1] << endl;

    for (int i = 0; i < intervals.size(); i++) {
        cout << "[" << intervals[i].start << ", " << intervals[i].end << "], " << endl;
    }
    cout << endl;
    system("pause");

    return 0;
}
