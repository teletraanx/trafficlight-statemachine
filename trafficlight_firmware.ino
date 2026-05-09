// Embedded Traffic Intersection Firmware with State Machine Control and UART Command Interface
// Arduino Uno R3

// South
const int S_RED = 13;     // PB5
const int S_YELLOW = 12;  // PB4
const int S_GREEN = 11;   // PB3

// East
const int E_RED = 10;
const int E_YELLOW = 9;
const int E_GREEN = 8;

// North
const int N_RED = 7;
const int N_YELLOW = 6;
const int N_GREEN = 5;

// West
const int W_RED = 4;
const int W_YELLOW = 3;
const int W_GREEN = 2;

const int LED_COUNT = 12;

const int pins[LED_COUNT] = {
  S_RED, S_YELLOW, S_GREEN,
  E_RED, E_YELLOW, E_GREEN,
  N_RED, N_YELLOW, N_GREEN,
  W_RED, W_YELLOW, W_GREEN
};

// State definitions
enum TrafficState {
  NS_GREEN_EW_RED,
  NS_YELLOW_EW_RED,
  NS_RED_EW_GREEN,
  NS_RED_EW_YELLOW
};

enum ControlMode {
  AUTO_MODE,
  MANUAL_MODE
};

// Function prototypes
void taskHandleCommands();
void taskUpdateController();

void allOff();
void applyState(TrafficState state);
void setSouthLightsDirect(bool redOn, bool yellowOn, bool greenOn);

unsigned long getStateDuration(TrafficState state);
TrafficState getNextState(TrafficState state);

void updateTrafficState();
void requestStateChange(TrafficState newState);

void handleSerialCommands();
void processCommand(String command);
void processStructuredCommand(String command);

String stateToString(TrafficState state);
String modeToString(ControlMode mode);

void printStatus();
void printHelp();

bool isYellowState(TrafficState state);
bool isStableState(TrafficState state);
bool parseState(String stateString, TrafficState &parsedState);

// Global state
TrafficState currentState = NS_GREEN_EW_RED;
ControlMode currentMode = AUTO_MODE;

bool manualModePending = false;

unsigned long lastStateChangeTime = 0;

const unsigned long GREEN_DURATION = 5000; // ms
const unsigned long YELLOW_DURATION = 2000;

// Arduino setup/loop
void setup() {
  Serial.begin(9600);

  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(pins[i], OUTPUT);
  }

  // Direct register setup for South LEDs
  // D13 = PB5 = South Red
  // D12 = PB4 = South Yellow
  // D11 = PB3 = South Green
  DDRB |= (1 << PB3) | (1 << PB4) | (1 << PB5);

  applyState(currentState);
  lastStateChangeTime = millis();

  Serial.println("Traffic controller ready.");
  printHelp();
}

void loop() {
  taskHandleCommands();
  taskUpdateController();
}

void taskHandleCommands() {
  handleSerialCommands();
}

void taskUpdateController() {
  if (currentMode == AUTO_MODE) {
    updateTrafficState();
  }
}

// LED control
void allOff() {
  for (int i = 0; i < LED_COUNT; i++) {
    digitalWrite(pins[i], LOW);
  }
}

void setSouthLightsDirect(bool redOn, bool yellowOn, bool greenOn) {
  if (redOn) {
    PORTB |= (1 << PB5);
  } else {
    PORTB &= ~(1 << PB5);
  }

  if (yellowOn) {
    PORTB |= (1 << PB4);
  } else {
    PORTB &= ~(1 << PB4);
  }

  if (greenOn) {
    PORTB |= (1 << PB3);
  } else {
    PORTB &= ~(1 << PB3);
  }
}

void applyState(TrafficState state) {
  allOff();

  Serial.print("TIME: ");
  Serial.print(millis());
  Serial.print(" | STATE: ");
  Serial.println(stateToString(state));

  switch (state) {
    case NS_GREEN_EW_RED:
      digitalWrite(N_GREEN, HIGH);
      setSouthLightsDirect(false, false, true);

      digitalWrite(E_RED, HIGH);
      digitalWrite(W_RED, HIGH);
      break;

    case NS_YELLOW_EW_RED:
      digitalWrite(N_YELLOW, HIGH);
      setSouthLightsDirect(false, true, false);

      digitalWrite(E_RED, HIGH);
      digitalWrite(W_RED, HIGH);
      break;

    case NS_RED_EW_GREEN:
      digitalWrite(N_RED, HIGH);
      setSouthLightsDirect(true, false, false);

      digitalWrite(E_GREEN, HIGH);
      digitalWrite(W_GREEN, HIGH);
      break;

    case NS_RED_EW_YELLOW:
      digitalWrite(N_RED, HIGH);
      setSouthLightsDirect(true, false, false);

      digitalWrite(E_YELLOW, HIGH);
      digitalWrite(W_YELLOW, HIGH);
      break;
  }
}

// State logic
unsigned long getStateDuration(TrafficState state) {
  switch (state) {
    case NS_GREEN_EW_RED:
    case NS_RED_EW_GREEN:
      return GREEN_DURATION;

    case NS_YELLOW_EW_RED:
    case NS_RED_EW_YELLOW:
      return YELLOW_DURATION;
  }

  return GREEN_DURATION;
}

TrafficState getNextState(TrafficState state) {
  switch (state) {
    case NS_GREEN_EW_RED:
      return NS_YELLOW_EW_RED;

    case NS_YELLOW_EW_RED:
      return NS_RED_EW_GREEN;

    case NS_RED_EW_GREEN:
      return NS_RED_EW_YELLOW;

    case NS_RED_EW_YELLOW:
      return NS_GREEN_EW_RED;
  }

  return NS_GREEN_EW_RED;
}

bool isYellowState(TrafficState state) {
  return state == NS_YELLOW_EW_RED || state == NS_RED_EW_YELLOW;
}

bool isStableState(TrafficState state) {
  return state == NS_GREEN_EW_RED || state == NS_RED_EW_GREEN;
}

void updateTrafficState() {
  unsigned long currentTime = millis();
  unsigned long stateDuration = getStateDuration(currentState);

  if (currentTime - lastStateChangeTime >= stateDuration) { // if current state's timing has elapsed
    currentState = getNextState(currentState);              // advance to next state
    applyState(currentState);
    lastStateChangeTime = currentTime;                      // reset time reference

    if (manualModePending && isStableState(currentState)) { 
      currentMode = MANUAL_MODE;
      manualModePending = false;
      Serial.println("OK: mode set to MANUAL after yellow transition completed");
    }
  }
}

void requestStateChange(TrafficState newState) {
  currentState = newState;
  applyState(currentState);

  if (isYellowState(currentState)) {  // temporarily keep mode on auto until yellow is done
    lastStateChangeTime = millis();
    currentMode = AUTO_MODE;
    manualModePending = true;
    Serial.println("OK: yellow state entered; controller will return to MANUAL after yellow completes");
  } else {
    Serial.print("OK: state set to ");
    Serial.println(stateToString(currentState));
  }
}

// Serial command handling
void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    processCommand(command);
  }
}

void processCommand(String command) {  // reject invalid commands before parsing
  if (!command.startsWith("CMD:")) {
    Serial.println("ERROR: invalid command format");
    return;
  }

  processStructuredCommand(command);
}

void processStructuredCommand(String command) { // CMD:<COMMAND>;ARG:<VALUE>
  if (command == "CMD:GET_STATUS") {            // get status
    printStatus();
  }
  else if (command == "CMD:HELP") {             // help
    printHelp();
  }
  else if (command == "CMD:SET_MODE;ARG:AUTO") { // set mode to AUTO
    manualModePending = false;
    currentMode = AUTO_MODE;
    lastStateChangeTime = millis();
    Serial.println("OK: mode set to AUTO");
  }
  else if (command == "CMD:SET_MODE;ARG:MANUAL") { // set mode to MANUAL
    if (currentMode == MANUAL_MODE) {
      manualModePending = false;
      Serial.println("OK: already in MANUAL mode");
    }
    else if (isStableState(currentState)) {
      currentMode = MANUAL_MODE;
      manualModePending = false;
      Serial.println("OK: mode set to MANUAL immediately");
    }
    else if (isYellowState(currentState)) {
      manualModePending = true;
      Serial.println("OK: MANUAL mode pending; yellow transition will complete first");
    }
  }
  else if (command == "CMD:NEXT") {                // in manual, advance to next state 
    if (currentMode == MANUAL_MODE) {
      requestStateChange(getNextState(currentState));
    } else {
      Serial.println("ERROR: NEXT only allowed in MANUAL mode");
    }
  }
  else if (command.startsWith("CMD:SET_STATE;ARG:")) {  // do not allow set_state in auto mode 
    if (currentMode != MANUAL_MODE) {
      Serial.println("ERROR: SET_STATE only allowed in MANUAL mode");
      return;
    }

    String stateText = command.substring(18);
    stateText.trim();

    TrafficState requestedState;
    if (parseState(stateText, requestedState)) {
      requestStateChange(requestedState);
    } else {
      Serial.println("ERROR: invalid state");             // reject invalid commands
    }
  }
  else {
    Serial.println("ERROR: unknown structured command");  // reject invalid commands
  }
}

// Utility
String stateToString(TrafficState state) {
  switch (state) {
    case NS_GREEN_EW_RED:
      return "NS_GREEN_EW_RED";
    case NS_YELLOW_EW_RED:
      return "NS_YELLOW_EW_RED";
    case NS_RED_EW_GREEN:
      return "NS_RED_EW_GREEN";
    case NS_RED_EW_YELLOW:
      return "NS_RED_EW_YELLOW";
  }

  return "UNKNOWN_STATE";
}

String modeToString(ControlMode mode) {
  switch (mode) {
    case AUTO_MODE:
      return "AUTO";
    case MANUAL_MODE:
      return "MANUAL";
  }

  return "UNKNOWN_MODE";
}

void printStatus() {
  Serial.print("STATUS | MODE: ");
  Serial.print(modeToString(currentMode));
  Serial.print(" | STATE: ");
  Serial.print(stateToString(currentState));
  Serial.print(" | MANUAL_PENDING: ");
  Serial.println(manualModePending ? "YES" : "NO");
}

void printHelp() {
  Serial.println("Structured protocol commands:");
  Serial.println("  CMD:HELP");
  Serial.println("  CMD:GET_STATUS");
  Serial.println("  CMD:SET_MODE;ARG:AUTO");
  Serial.println("  CMD:SET_MODE;ARG:MANUAL");
  Serial.println("  CMD:NEXT");
  Serial.println("  CMD:SET_STATE;ARG:NS_GREEN_EW_RED");
  Serial.println("  CMD:SET_STATE;ARG:NS_YELLOW_EW_RED");
  Serial.println("  CMD:SET_STATE;ARG:NS_RED_EW_GREEN");
  Serial.println("  CMD:SET_STATE;ARG:NS_RED_EW_YELLOW");
}

bool parseState(String stateString, TrafficState &parsedState) {    // return false if invalid state
  if (stateString == "NS_GREEN_EW_RED") {
    parsedState = NS_GREEN_EW_RED;
    return true;
  }
  else if (stateString == "NS_YELLOW_EW_RED") {
    parsedState = NS_YELLOW_EW_RED;
    return true;
  }
  else if (stateString == "NS_RED_EW_GREEN") {
    parsedState = NS_RED_EW_GREEN;
    return true;
  }
  else if (stateString == "NS_RED_EW_YELLOW") {
    parsedState = NS_RED_EW_YELLOW;
    return true;
  }

  return false;
}
