pragma solidity ^0.4.8;

contract EventManager {
    uint8 public currentTemperature;
    
    function setTemperature(uint8 temperature) public {
        currentTemperature = temperature;
    }
}