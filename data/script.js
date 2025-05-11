let currentUser = "Default_User";
let targetTemperature = 22.0;

function adjustTemperature(change) {
    const tempInput = document.getElementById("tempInput");
    let newTemp = parseInt(tempInput.value) + change;
    
    newTemp = Math.max(10, Math.min(40, newTemp));
    
    tempInput.value = newTemp;
    setTargetTemperature(newTemp);
}

function setTargetTemperature(temp) {
    targetTemperature = parseFloat(temp);
    document.getElementById("targetTemp").textContent = targetTemperature;
    fetch(`/api/target_temp?temp=${targetTemperature}&user=${currentUser}`)
        .then(response => response.text())
        .then(data => console.log("Target temp set:", data))
        .catch(err => console.error("Error setting target temp:", err));
}

function updateTemperatureDisplay() {
    fetch("/api/current_temp")
        .then(response => response.text())
        .then(temp => {
            document.getElementById("currentTemp").textContent = temp;
        })
        .catch(err => console.error("Temperature error:", err));
}

function addUser(username, action, temp) {
    if (!username) return;
    fetch(`/api/user?user=${username}&action=${action}&temp=${temp}`)
        .then(response => response.text())
        .then(data => {
            console.log("User management response:", data);
            alert(`User ${action}ed successfully.`);
            document.getElementById("newUser").value = "";
            document.getElementById("newUserTemp").value = "22";
            loadUsers();
        })
        .catch(err => {
            console.error("User management error:", err);
            alert("Failed to " + action + " user.");
        });
}

function switchUser(username) {
    if (!username) return;
    fetch(`/api/switch?user=${username}`)
        .then(response => response.text())
        .then(data => {
            console.log("Switch user response:", data);
            currentUser = username;
            updateCurrentUser();
            loadUsers();
            document.getElementById("switchUser").value = "";
        })
        .catch(err => {
            console.error("Switch user error:", err);
            alert("Failed to switch user.");
        });
}

function updateCurrentUser() {
    fetch("/api/current_user")
        .then(response => response.text())
        .then(user => {
            currentUser = user;
            document.getElementById("currentUserDisplay").textContent = user;
            loadUsers();
        })
        .catch(err => console.error("Current user error:", err));
}

function loadUsers() {
    fetch("/api/users")
        .then(response => response.json())
        .then(users => {
            const userList = document.getElementById("userList");
            userList.innerHTML = "";
            users.forEach(user => {
                const li = document.createElement("li");
                li.textContent = `${user.name} (${user.temp}°C)`;
                if (user.name === currentUser) {
                    li.style.fontWeight = "bold";
                    document.getElementById("targetTemp").textContent = user.temp;
                    document.getElementById("tempInput").value = user.temp;
                    targetTemperature = user.temp;
                }
                userList.appendChild(li);
            });
        })
        .catch(err => console.error("User list error:", err));
}

setInterval(updateTemperatureDisplay, 5000);

window.onload = function() {
    updateCurrentUser();
    loadUsers();
    updateTemperatureDisplay();
};